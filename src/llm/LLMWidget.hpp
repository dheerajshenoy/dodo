#pragma once

#include "../Config.hpp"
#include "providers/LLMProvider.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QWidget>

class LLMWidget : public QWidget
{
    Q_OBJECT
public:
    LLMWidget(const Config &config, QWidget *parent = nullptr) noexcept;

private:
    void initGui() noexcept;
    void sendQuery() noexcept;
    void initProvider() noexcept;

    QTextEdit *m_chat_edit{nullptr};
    QTextEdit *m_input_edit{nullptr};
    QPushButton *m_send_btn{nullptr};
    Config m_config;

    LLM::Provider *m_provider{nullptr};
    bool m_stream_in_progress{false};
};
