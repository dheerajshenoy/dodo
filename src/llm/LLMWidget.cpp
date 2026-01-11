#include "LLMWidget.hpp"

#include "providers/ollama/OllamaProvider.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QTextCursor>

LLMWidget::LLMWidget(const Config &config, QWidget *parent) noexcept
    : QWidget(parent), m_config(config)
{
    initGui();
    initProvider();
}

void
LLMWidget::initProvider() noexcept
{
    if (m_config.llm.provider == "ollama")
    {
        m_provider = new OllamaProvider();

        // const std::string systemPrompt
        //     = "* You are a helpful assistant embedded in a PDF reader app "
        //       "named dodo."
        //       "* You may request actions by outputting a single JSON object"
        //       "* Allowed commands:"
        //       "** goto_page(page:int)"
        //       "** search(query:string, case_sensitive:bool)"
        //       "** get_current_page()"
        //       "** get_page_text(page:int, max_chars:int)"
        //       "** add_note(page:int, text:string)"
        //       "* Always format your responses in markdown rich format for "
        //       "bold, "
        //       "italic etc..";

        // m_provider->setSystemPrompt(systemPrompt);
    }
    else
    {
        m_chat_edit->append("<b>LLM error:</b> Unsupported provider: "
                            + QString::fromStdString(m_config.llm.provider));
        return;
    }

    connect(m_provider, &LLM::Provider::dataReceived, this,
            [this](const std::string &data)
    {
        if (data.empty())
            return;

        const QString token = QString::fromStdString(data);
        auto cursor         = m_chat_edit->textCursor();
        cursor.movePosition(QTextCursor::End);
        if (!m_stream_in_progress)
        {
            cursor.insertBlock();
            cursor.insertHtml("<b>LLM:</b> ");
            m_stream_in_progress = true;
        }
        cursor.insertText(token);
        m_chat_edit->setTextCursor(cursor);
        m_chat_edit->ensureCursorVisible();
    });

    connect(m_provider, &LLM::Provider::streamFinished, this, [this]()
    {
        if (m_stream_in_progress)
        {
            m_chat_edit->append("");
            m_stream_in_progress = false;
        }
    });

    connect(m_provider, &LLM::Provider::requestFailed, this,
            [this](const std::string &error)
    {
        m_chat_edit->append("<b>LLM error:</b> "
                            + QString::fromStdString(error));
        m_stream_in_progress = false;
    });
}

void
LLMWidget::sendQuery() noexcept
{
    const QString user_input = m_input_edit->toPlainText().trimmed();

    if (user_input.isEmpty())
        return;

    m_chat_edit->append("<b>User:</b> " + user_input);
    m_input_edit->clear();
    m_stream_in_progress = false;
    LLM::Request llm_request{.model      = m_config.llm.model,
                             .prompt     = user_input.toStdString(),
                             .max_tokens = m_config.llm.max_tokens};
    m_provider->chat_stream(llm_request);
}

void
LLMWidget::initGui() noexcept
{
    m_chat_edit  = new QTextEdit(this);
    m_input_edit = new QTextEdit(this);
    m_send_btn   = new QPushButton("Send", this);

    m_chat_edit->setAcceptRichText(true);
    m_chat_edit->setReadOnly(true);

    m_input_edit->setPlaceholderText("Enter your message...");
    m_input_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    auto *layout = new QVBoxLayout(this);

    auto *input_layout = new QHBoxLayout();
    input_layout->addWidget(m_input_edit);
    input_layout->addWidget(m_send_btn);

    layout->addWidget(m_chat_edit);
    layout->addLayout(input_layout);

    setLayout(layout);

    connect(m_send_btn, &QPushButton::clicked, this, &LLMWidget::sendQuery);
}
