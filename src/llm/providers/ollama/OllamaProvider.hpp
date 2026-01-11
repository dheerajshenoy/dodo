#pragma once

#include "http/HttpStreamClient.hpp"
#include "providers/LLMProvider.hpp"

#include <QObject>

class OllamaProvider : public LLM::Provider
{
    Q_OBJECT
public:
    OllamaProvider();
    ~OllamaProvider();
    void chat_stream(const LLM::Request &request) override;

private:
    HttpStreamClient *m_client{nullptr};
};
