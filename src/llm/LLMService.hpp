#pragma once

#include "ActionHandler.hpp"
#include "CopilotProvider.hpp"
#include "LLMProvider.hpp"

#include <QMap>
#include <QObject>
#include <QString>
#include <memory>

// LLMService manages LLM providers and acts as the main interface
// for the application to interact with language models
class LLMService : public QObject
{
    Q_OBJECT

public:
    explicit LLMService(QObject *parent = nullptr)
        : QObject(parent)
        , m_actionHandler(new ActionHandler(this))
    {
        // Register available providers
        registerProvider(std::make_unique<CopilotProvider>(this));

        // Set default provider
        setActiveProvider("copilot");
    }

    ~LLMService() = default;

    // Provider management
    QStringList availableProviders() const
    {
        return m_providers.keys();
    }

    QString activeProviderName() const
    {
        return m_activeProvider ? m_activeProvider->name() : QString();
    }

    LLMProvider *activeProvider() const
    {
        return m_activeProvider;
    }

    bool setActiveProvider(const QString &name)
    {
        if (!m_providers.contains(name))
            return false;

        m_activeProvider = m_providers[name];
        emit activeProviderChanged(name);
        return true;
    }

    // Configuration
    void setApiKey(const QString &providerName, const QString &apiKey)
    {
        if (m_providers.contains(providerName))
            m_providers[providerName]->setApiKey(apiKey);
    }

    void setModel(const QString &model)
    {
        if (m_activeProvider)
            m_activeProvider->setModel(model);
    }

    QString model() const
    {
        return m_activeProvider ? m_activeProvider->model() : QString();
    }

    QStringList availableModels() const
    {
        return m_activeProvider ? m_activeProvider->availableModels()
                                : QStringList();
    }

    bool isConfigured() const
    {
        return m_activeProvider && m_activeProvider->isConfigured();
    }

    bool isLoading() const
    {
        return m_activeProvider && m_activeProvider->isLoading();
    }

    // Action handler
    ActionHandler *actionHandler() const
    {
        return m_actionHandler;
    }

    // Enable/disable actions
    void setActionsEnabled(bool enabled)
    {
        m_actionsEnabled = enabled;
    }

    bool actionsEnabled() const
    {
        return m_actionsEnabled;
    }

    // Conversation management
    void setSystemPrompt(const QString &prompt)
    {
        m_systemPrompt = prompt;
    }

    QString systemPrompt() const
    {
        return m_systemPrompt;
    }

    void clearConversation()
    {
        m_conversationHistory.clear();
        emit conversationCleared();
    }

    const QList<ChatMessage> &conversationHistory() const
    {
        return m_conversationHistory;
    }

    // Send a message to the LLM
    void sendMessage(const QString &userMessage, const QString &context = QString())
    {
        if (!m_activeProvider)
        {
            emit errorOccurred("No LLM provider configured");
            return;
        }

        if (!m_activeProvider->isConfigured())
        {
            emit errorOccurred("LLM provider not properly configured. "
                               "Please set your API key.");
            return;
        }

        // Build message list
        QList<ChatMessage> messages;

        // Build system content
        QString systemContent;
        
        if (!m_systemPrompt.isEmpty())
        {
            systemContent = m_systemPrompt;
        }
        else
        {
            systemContent = "You are a helpful assistant. The user is reading a PDF document.";
        }

        // Add action instructions if enabled
        if (m_actionsEnabled && m_actionHandler->isReady())
        {
            systemContent += "\n\n" + m_actionHandler->getActionsPrompt();
        }

        // Add document context
        if (!context.isEmpty())
        {
            systemContent += "\n\n--- Document Context ---\n" + context;
        }

        messages.append({ChatMessage::Role::System, systemContent});

        // Add conversation history
        messages.append(m_conversationHistory);

        // Add the new user message
        ChatMessage userMsg{ChatMessage::Role::User, userMessage};
        messages.append(userMsg);
        m_conversationHistory.append(userMsg);

        // Send to provider
        m_activeProvider->sendMessage(messages);
    }

    // Cancel ongoing request
    void cancelRequest()
    {
        if (m_activeProvider)
            m_activeProvider->cancelRequest();
    }

signals:
    void activeProviderChanged(const QString &providerName);
    void responseReceived(const QString &response);
    void streamingResponse(const QString &partialResponse);
    void errorOccurred(const QString &error);
    void loadingChanged(bool isLoading);
    void conversationCleared();
    void actionExecuted(const ActionHandler::ActionResult &result);

private:
    QMap<QString, LLMProvider *> m_providers;
    LLMProvider *m_activeProvider{nullptr};
    ActionHandler *m_actionHandler{nullptr};
    QString m_systemPrompt;
    QList<ChatMessage> m_conversationHistory;
    bool m_actionsEnabled{true};

    void registerProvider(std::unique_ptr<LLMProvider> provider)
    {
        QString name = provider->name();
        LLMProvider *rawPtr = provider.release();
        m_providers[name] = rawPtr;

        // Connect provider signals
        connect(rawPtr, &LLMProvider::responseReceived, this,
                [this](const QString &response) {
                    // Add assistant response to history
                    m_conversationHistory.append(
                        {ChatMessage::Role::Assistant, response});

                    // Process any actions in the response
                    if (m_actionsEnabled && m_actionHandler->isReady())
                    {
                        auto results = m_actionHandler->parseAndExecuteActions(response);
                        for (const auto &result : results)
                        {
                            emit actionExecuted(result);
                        }
                    }

                    emit responseReceived(response);
                });

        connect(rawPtr, &LLMProvider::streamingResponse, this,
                &LLMService::streamingResponse);

        connect(rawPtr, &LLMProvider::errorOccurred, this,
                &LLMService::errorOccurred);

        connect(rawPtr, &LLMProvider::loadingChanged, this,
                &LLMService::loadingChanged);
    }
};