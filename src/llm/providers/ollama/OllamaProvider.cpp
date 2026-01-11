#include "OllamaProvider.hpp"

#include "../json.hpp"

OllamaProvider::OllamaProvider() : LLM::Provider()
{
    m_client = new HttpStreamClient();
    m_client->setURL("http://localhost:11434/api/chat");

    connect(m_client, &HttpStreamClient::dataReceived, this,
            [this](const std::string &data)
    {
        auto j = nlohmann::json::parse(data, nullptr, false);
        if (j.is_discarded())
            return;

        if (j.contains("error"))
        {
            emit requestFailed(j["error"].get<std::string>());
            return;
        }

        if (j.contains("done") && j["done"].get<bool>())
        {
            emit streamFinished();
            return;
        }

        if (j.contains("message") && j["message"].contains("content"))
            emit dataReceived(j["message"]["content"].get<std::string>());
    });
}

OllamaProvider::~OllamaProvider()
{
    delete m_client;
}

void
OllamaProvider::chat_stream(const LLM::Request &request)
{
    const std::string content = m_system_prompt.empty()
                                    ? request.prompt
                                    : m_system_prompt + "\n\n" + request.prompt;
    nlohmann::json j;
    j["model"]    = request.model;
    j["stream"]   = true;
    j["messages"] = nlohmann::json::array();
    j["messages"].push_back({{"role", "user"}, {"content", content}});
    j["max_tokens"] = request.max_tokens;
    // j["temperature"] = request.temperature;
    const std::string data = j.dump();

    m_client->sendRequest(data);
}
