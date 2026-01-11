#pragma once

#include <QObject>
#include <string>

namespace LLM
{

struct Request
{
    std::string model;
    std::string prompt;
    int max_tokens{512};
    // float temperature{0.7f}; //
};

class Provider : public QObject
{
    Q_OBJECT
public:
    explicit Provider(QObject *parent = nullptr);
    ~Provider() override;

    inline void setSystemPrompt(const std::string &prompt)
    {
        m_system_prompt = prompt;
    }

    virtual void chat_stream(const Request &request) = 0;

protected:
    std::string m_system_prompt{};

signals:
    void dataReceived(const std::string &data);
    void requestFailed(const std::string &error);
    void streamFinished();
}; // namespace LLM
}; // namespace LLM
