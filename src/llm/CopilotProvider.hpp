#pragma once

#include "LLMProvider.hpp"

#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

// GitHub Copilot provider using GitHub Models API
// 
// Authentication options:
// 1. GitHub Personal Access Token (classic or fine-grained)
//    - Go to https://github.com/settings/tokens
//    - Create a token (no special scopes needed for public models)
// 
// 2. GitHub Models Token
//    - Go to https://github.com/marketplace/models
//    - Select a model and get a token from the playground
//
// Set the token via:
// - Config file: github_token = "your-token"
// - Environment variable: GITHUB_TOKEN
// - Or enter directly in the chat panel
class CopilotProvider : public LLMProvider
{
    Q_OBJECT

public:
    explicit CopilotProvider(QObject *parent = nullptr)
        : LLMProvider(parent)
    {
        // Default to GPT-4o model
        m_model = "gpt-4o";
    }

    QString name() const override
    {
        return "copilot";
    }

    QString displayName() const override
    {
        return "GitHub Copilot";
    }

    QStringList availableModels() const override
    {
        return {
            "gpt-4o",
            "gpt-4o-mini", 
            "gpt-4.1",
            "gpt-4.1-mini",
            "gpt-4.1-nano",
            "o1",
            "o1-mini",
            "o1-preview",
            "o3-mini",
        };
    }

    void sendMessage(const QList<ChatMessage> &messages) override
    {
        if (!isConfigured())
        {
            emit errorOccurred(
                "GitHub Models token not configured.\n\n"
                "To get a token:\n"
                "1. Go to https://github.com/marketplace/models\n"
                "2. Select a model (e.g., GPT-4o)\n"
                "3. Click 'Playground' and copy the token from the code sample\n\n"
                "Note: Requires GitHub Copilot subscription.\n"
                "Or set GITHUB_TOKEN environment variable.");
            return;
        }

        if (m_currentReply)
        {
            m_currentReply->abort();
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }

        // GitHub Models API endpoint (Azure-hosted)
        QUrl url("https://models.inference.ai.azure.com/chat/completions");

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization",
                             QString("Bearer %1").arg(m_apiKey).toUtf8());
        // Add GitHub-specific headers that might help with auth
        request.setRawHeader("X-GitHub-Api-Version", "2024-12-01");
        request.setRawHeader("User-Agent", "dodo-pdf-reader/1.0");

        // Build the request body
        QJsonArray messagesArray;
        for (const auto &msg : messages)
            messagesArray.append(msg.toJson());

        QJsonObject body;
        body["model"]    = m_model;
        body["messages"] = messagesArray;
        body["stream"]   = true;

        // Add reasonable defaults
        body["temperature"] = 0.7;
        body["max_tokens"]  = 4096;

        QJsonDocument doc(body);
        QByteArray data = doc.toJson();

        m_currentReply = m_networkManager->post(request, data);
        m_streamBuffer.clear();
        m_fullResponse.clear();
        setLoading(true);

        connect(m_currentReply, &QNetworkReply::readyRead, this, [this]() {
            processStreamingResponse();
        });

        connect(m_currentReply, &QNetworkReply::finished, this, [this]() {
            setLoading(false);

            if (!m_currentReply)
                return;

            if (m_currentReply->error() != QNetworkReply::NoError)
            {
                // Check if it was cancelled
                if (m_currentReply->error()
                    == QNetworkReply::OperationCanceledError)
                {
                    m_currentReply->deleteLater();
                    m_currentReply = nullptr;
                    return;
                }

                QString errorMsg   = m_currentReply->errorString();
                int httpStatus     = m_currentReply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute).toInt();
                QByteArray responseData = m_currentReply->readAll();

                // Try to parse error from response
                QJsonDocument errorDoc
                    = QJsonDocument::fromJson(responseData);
                if (errorDoc.isObject())
                {
                    QJsonObject errorObj = errorDoc.object();
                    if (errorObj.contains("error"))
                    {
                        QJsonObject error = errorObj["error"].toObject();
                        if (error.contains("message"))
                            errorMsg = error["message"].toString();
                    }
                    else if (errorObj.contains("message"))
                    {
                        errorMsg = errorObj["message"].toString();
                    }
                }

                // Provide helpful error messages based on status code
                if (httpStatus == 401 || httpStatus == 403)
                {
                    errorMsg = QString(
                        "Authentication failed (HTTP %1).\n\n"
                        "Please check your GitHub Models token:\n"
                        "1. Go to https://github.com/marketplace/models\n"
                        "2. Select a model and open the Playground\n"
                        "3. Copy the token from the code sample\n\n"
                        "Note: You need a GitHub Copilot subscription.\n\n"
                        "Original error: %2")
                        .arg(httpStatus)
                        .arg(errorMsg);
                }
                else if (httpStatus == 404)
                {
                    errorMsg = QString(
                        "Model not found (HTTP 404).\n\n"
                        "The model '%1' may not be available.\n"
                        "Try using 'gpt-4o' or 'gpt-4o-mini'.\n\n"
                        "Original error: %2")
                        .arg(m_model)
                        .arg(errorMsg);
                }
                else if (httpStatus == 429)
                {
                    errorMsg = "Rate limit exceeded. Please wait a moment and try again.";
                }

                emit errorOccurred(errorMsg);
            }
            else if (!m_fullResponse.isEmpty())
            {
                emit responseReceived(m_fullResponse);
            }

            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        });
    }

private:
    QString m_streamBuffer;
    QString m_fullResponse;

    void processStreamingResponse()
    {
        if (!m_currentReply)
            return;

        m_streamBuffer += QString::fromUtf8(m_currentReply->readAll());

        // Process complete SSE lines
        while (true)
        {
            int newlinePos = m_streamBuffer.indexOf('\n');
            if (newlinePos == -1)
                break;

            QString line   = m_streamBuffer.left(newlinePos).trimmed();
            m_streamBuffer = m_streamBuffer.mid(newlinePos + 1);

            if (line.isEmpty())
                continue;

            // SSE format: "data: {json}"
            if (line.startsWith("data: "))
            {
                QString jsonStr = line.mid(6);  // Remove "data: " prefix

                if (jsonStr == "[DONE]")
                    continue;

                QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
                if (!doc.isObject())
                    continue;

                QJsonObject obj     = doc.object();
                QJsonArray choices  = obj["choices"].toArray();
                if (choices.isEmpty())
                    continue;

                QJsonObject choice = choices[0].toObject();
                QJsonObject delta  = choice["delta"].toObject();

                if (delta.contains("content"))
                {
                    QString content = delta["content"].toString();
                    m_fullResponse += content;
                    emit streamingResponse(m_fullResponse);
                }
            }
        }
    }
};