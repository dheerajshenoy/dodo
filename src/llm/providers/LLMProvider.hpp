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

    virtual void chat_stream(const Request &request) = 0;

signals:
    void dataReceived(const std::string &data);
    void requestFailed(const std::string &error);
    void streamFinished();
};

}; // namespace LLM
