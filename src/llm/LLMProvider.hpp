#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

// Message structure for chat conversations
struct ChatMessage
{
    enum class Role
    {
        System,
        User,
        Assistant
    };

    Role role;
    QString content;

    static QString roleToString(Role r)
    {
        switch (r)
        {
            case Role::System:
                return "system";
            case Role::User:
                return "user";
            case Role::Assistant:
                return "assistant";
        }
        return "user";
    }

    QJsonObject toJson() const
    {
        return QJsonObject{{"role", roleToString(role)}, {"content", content}};
    }
};

// Abstract base class for LLM providers
class LLMProvider : public QObject
{
    Q_OBJECT

public:
    explicit LLMProvider(QObject *parent = nullptr)
        : QObject(parent)
        , m_networkManager(new QNetworkAccessManager(this))
    {
    }

    virtual ~LLMProvider() = default;

    // Provider identification
    virtual QString name() const = 0;
    virtual QString displayName() const = 0;

    // Configuration
    virtual void setApiKey(const QString &apiKey)
    {
        m_apiKey = apiKey;
    }
    virtual void setModel(const QString &model)
    {
        m_model = model;
    }
    virtual QString model() const
    {
        return m_model;
    }

    // Check if provider is properly configured
    virtual bool isConfigured() const
    {
        return !m_apiKey.isEmpty();
    }

    // Available models for this provider
    virtual QStringList availableModels() const = 0;

    // Send a chat completion request
    virtual void sendMessage(const QList<ChatMessage> &messages) = 0;

    // Cancel ongoing request
    virtual void cancelRequest()
    {
        if (m_currentReply)
        {
            m_currentReply->abort();
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }
    }

    // Check if a request is in progress
    bool isLoading() const
    {
        return m_currentReply != nullptr;
    }

signals:
    // Emitted when a complete response is received
    void responseReceived(const QString &response);

    // Emitted for streaming responses (partial content)
    void streamingResponse(const QString &partialResponse);

    // Emitted when an error occurs
    void errorOccurred(const QString &error);

    // Emitted when request starts/ends
    void loadingChanged(bool isLoading);

protected:
    QNetworkAccessManager *m_networkManager{nullptr};
    QNetworkReply *m_currentReply{nullptr};
    QString m_apiKey;
    QString m_model;

    void setLoading(bool loading)
    {
        emit loadingChanged(loading);
    }
};