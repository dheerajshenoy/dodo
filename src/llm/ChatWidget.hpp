#pragma once

#include "ActionHandler.hpp"
#include "ContextManager.hpp"
#include "LLMService.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTextEdit>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

// Custom text edit that sends on Enter (Shift+Enter for newline)
class ChatInputEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit ChatInputEdit(QWidget *parent = nullptr)
        : QTextEdit(parent)
    {
        setPlaceholderText("Ask about the document... (Enter to send, Shift+Enter for newline)");
        setMaximumHeight(100);
        setAcceptRichText(false);
    }

signals:
    void sendRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        {
            if (event->modifiers() & Qt::ShiftModifier)
            {
                // Shift+Enter: insert newline
                QTextEdit::keyPressEvent(event);
            }
            else
            {
                // Enter: send message
                emit sendRequested();
            }
        }
        else
        {
            QTextEdit::keyPressEvent(event);
        }
    }
};

// Widget to display a single chat message
class ChatMessageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatMessageWidget(const QString &content, bool isUser,
                               QWidget *parent = nullptr)
        : QWidget(parent)
        , m_isUser(isUser)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 4, 8, 4);

        // Role label
        auto *roleLabel = new QLabel(isUser ? "You" : "Assistant", this);
        roleLabel->setStyleSheet(
            QString("font-weight: bold; color: %1;")
                .arg(isUser ? "#4a9eff" : "#10b981"));

        // Content label
        m_contentLabel = new QLabel(this);
        m_contentLabel->setWordWrap(true);
        m_contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_contentLabel->setStyleSheet("padding: 8px; border-radius: 8px; "
                                      "background-color: palette(alternate-base);");
        setContent(content);

        layout->addWidget(roleLabel);
        layout->addWidget(m_contentLabel);
    }

    void setContent(const QString &content)
    {
        // Convert markdown-like formatting to HTML
        QString html = content.toHtmlEscaped();
        html.replace("\n", "<br>");
        
        // Basic code block handling
        html.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");
        
        m_contentLabel->setText(html);
    }

    bool isUser() const { return m_isUser; }

private:
    QLabel *m_contentLabel;
    bool m_isUser;
};

// Main chat widget with enhanced context management
class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(LLMService *service, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_service(service)
        , m_contextManager(new ContextManager(this))
    {
        setupUi();
        connectSignals();
        updateConfigurationState();
        
        // If API key is already configured, show placeholder instead
        if (m_service->isConfigured())
        {
            m_apiKeyEdit->setPlaceholderText("Token configured (from config or env)");
            m_apiKeyEdit->setText("");
            m_configWidget->setVisible(false);  // Hide config when pre-configured
        }
        else
        {
            // Show helpful message for first-time users
            addMessage("Welcome! To use AI features, you need a GitHub Models token.\n\n"
                       "To get your token:\n"
                       "1. Go to github.com/marketplace/models\n"
                       "2. Select a model (e.g., GPT-4o)\n"
                       "3. Click 'Get started' or 'Playground'\n"
                       "4. Copy the token from the code sample\n\n"
                       "Note: You need a GitHub Copilot subscription for access.", false);
        }

        // Show example prompts
        addMessage("💡 **Example questions you can ask:**\n\n"
                   "**Navigation:**\n"
                   "• \"Take me to equation 3.2\"\n"
                   "• \"Show me figure 5\"\n"
                   "• \"Go to the section about methodology\"\n"
                   "• \"Find where they discuss neural networks\"\n\n"
                   "**Understanding:**\n"
                   "• \"Explain this equation\"\n"
                   "• \"Summarize this page\"\n"
                   "• \"What are the key findings?\"\n"
                   "• \"What does this term mean?\"\n\n"
                   "The AI can navigate the document for you automatically!", false);
    }

    // Get the context manager for external configuration
    ContextManager *contextManager() const { return m_contextManager; }

    // Set context directly (legacy method, prefer using ContextManager)
    void setContext(const QString &context)
    {
        m_currentContext = context;
    }

    void setSelectedText(const QString &text)
    {
        m_selectedText = text;
    }

    // Set the document view for context extraction
    void setDocumentView(DocumentView *docView)
    {
        m_contextManager->setDocumentView(docView);
        m_service->actionHandler()->setDocumentView(docView);
        updateContextInfo();
    }
    
    // Show/hide the configuration panel
    void setConfigurationVisible(bool visible)
    {
        m_configWidget->setVisible(visible);
    }

    // Show/hide context settings panel
    void setContextSettingsVisible(bool visible)
    {
        m_contextSettingsWidget->setVisible(visible);
    }

public slots:
    void askAboutSelection()
    {
        if (m_selectedText.isEmpty())
            return;

        m_inputEdit->setPlainText(
            QString("Please explain the following text:\n\n\"%1\"")
                .arg(m_selectedText));
        m_inputEdit->setFocus();
    }

    void summarizePage()
    {
        sendMessage("Please summarize this page concisely.");
    }

    void summarizeDocument()
    {
        // Temporarily switch to full document mode
        auto oldMode = m_contextManager->settings().mode;
        m_contextManager->setContextMode(ContextManager::ContextMode::FullDocument);
        
        sendMessage("Please provide a comprehensive summary of this entire document.");
        
        // Restore previous mode
        m_contextManager->setContextMode(oldMode);
    }

    void explainConcepts()
    {
        sendMessage("What are the key concepts and terms explained in this section?");
    }

    void findEquationPrompt()
    {
        m_inputEdit->setPlainText("Take me to equation ");
        m_inputEdit->setFocus();
        m_inputEdit->moveCursor(QTextCursor::End);
    }

    void findFigurePrompt()
    {
        m_inputEdit->setPlainText("Show me figure ");
        m_inputEdit->setFocus();
        m_inputEdit->moveCursor(QTextCursor::End);
    }

    void findSectionPrompt()
    {
        m_inputEdit->setPlainText("Go to the section about ");
        m_inputEdit->setFocus();
        m_inputEdit->moveCursor(QTextCursor::End);
    }

private slots:
    void sendMessage()
    {
        QString text = m_inputEdit->toPlainText().trimmed();
        if (text.isEmpty())
            return;

        sendMessage(text);
    }

    void sendMessage(const QString &text)
    {
        // Add user message to UI
        addMessage(text, true);

        // Clear input
        m_inputEdit->clear();

        // Create placeholder for assistant response
        m_currentAssistantMessage = new ChatMessageWidget("...", false, this);
        m_messagesLayout->addWidget(m_currentAssistantMessage);
        scrollToBottom();

        // Get context from manager or use legacy context
        QString context = m_contextManager->getContext();
        if (context.isEmpty() && !m_currentContext.isEmpty())
        {
            context = m_currentContext;
        }

        // Send to LLM
        m_service->sendMessage(text, context);
    }

    void onStreamingResponse(const QString &response)
    {
        if (m_currentAssistantMessage)
        {
            m_currentAssistantMessage->setContent(response);
            scrollToBottom();
        }
    }

    void onResponseReceived(const QString &response)
    {
        if (m_currentAssistantMessage)
        {
            m_currentAssistantMessage->setContent(response);
            m_currentAssistantMessage = nullptr;
            scrollToBottom();
        }
    }

    void onError(const QString &error)
    {
        if (m_currentAssistantMessage)
        {
            m_currentAssistantMessage->setContent("Error: " + error);
            m_currentAssistantMessage = nullptr;
        }
        m_statusLabel->setText("Error: " + error);
        m_statusLabel->setStyleSheet("color: #ef4444;");
    }

    void onLoadingChanged(bool loading)
    {
        m_sendButton->setEnabled(!loading);
        m_cancelButton->setVisible(loading);
        m_inputEdit->setEnabled(!loading);

        if (loading)
        {
            m_statusLabel->setText("Thinking...");
            m_statusLabel->setStyleSheet("color: #f59e0b;");
        }
        else
        {
            m_statusLabel->setText("Ready");
            m_statusLabel->setStyleSheet("color: #10b981;");
        }
    }

    void clearConversation()
    {
        // Remove all message widgets
        while (m_messagesLayout->count() > 0)
        {
            QLayoutItem *item = m_messagesLayout->takeAt(0);
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }

        m_currentAssistantMessage = nullptr;
        m_service->clearConversation();
    }

    void onApiKeyChanged()
    {
        QString apiKey = m_apiKeyEdit->text().trimmed();
        m_service->setApiKey("copilot", apiKey);
        updateConfigurationState();
    }

    void onContextModeChanged(int index)
    {
        auto mode = static_cast<ContextManager::ContextMode>(index);
        m_contextManager->setContextMode(mode);
        updateContextInfo();
        
        // Show/hide surrounding pages spinner based on mode
        m_surroundingPagesSpinBox->setEnabled(
            mode == ContextManager::ContextMode::SurroundingPages ||
            mode == ContextManager::ContextMode::SmartChunking);
    }

    void onSurroundingPagesChanged(int value)
    {
        m_contextManager->setSurroundingPages(value);
        updateContextInfo();
    }

    void onIncludeMetadataChanged(bool checked)
    {
        m_contextManager->setIncludeMetadata(checked);
        updateContextInfo();
    }

    void onIncludeOutlineChanged(bool checked)
    {
        m_contextManager->setIncludeOutline(checked);
        updateContextInfo();
    }

    void onAutoUpdateChanged(bool checked)
    {
        m_contextManager->setAutoUpdate(checked);
    }

    void updateContextInfo()
    {
        int tokenCount = m_contextManager->estimatedTokenCount();
        m_contextInfoLabel->setText(
            QString("Context: ~%1 tokens").arg(tokenCount));
        
        // Warn if context is very large
        if (tokenCount > 10000)
        {
            m_contextInfoLabel->setStyleSheet("color: #f59e0b;");
            m_contextInfoLabel->setToolTip("Large context may slow down responses");
        }
        else
        {
            m_contextInfoLabel->setStyleSheet("color: #6b7280;");
            m_contextInfoLabel->setToolTip("");
        }
    }

    void toggleContextSettings()
    {
        bool visible = m_contextSettingsWidget->isVisible();
        m_contextSettingsWidget->setVisible(!visible);
        m_toggleContextSettingsButton->setText(visible ? "▼ Context Settings" : "▲ Context Settings");
    }

    void onEnableActionsChanged(bool checked)
    {
        m_service->setActionsEnabled(checked);
    }

    void onActionExecuted(const ActionHandler::ActionResult &result)
    {
        // Show action result as a system message
        QString statusIcon = result.success ? "✓" : "✗";
        QString message = QString("<i>%1 Action: %2</i>")
                              .arg(statusIcon)
                              .arg(result.message.toHtmlEscaped());
        
        auto *actionLabel = new QLabel(message, this);
        actionLabel->setWordWrap(true);
        actionLabel->setStyleSheet(
            result.success 
                ? "color: #10b981; font-size: 11px; padding: 4px 8px; background-color: rgba(16, 185, 129, 0.1); border-radius: 4px;"
                : "color: #ef4444; font-size: 11px; padding: 4px 8px; background-color: rgba(239, 68, 68, 0.1); border-radius: 4px;");
        m_messagesLayout->addWidget(actionLabel);
        scrollToBottom();
    }

    void onNavigationRequested(int pageNo, const QString &reason)
    {
        Q_UNUSED(pageNo);
        m_statusLabel->setText(reason);
        m_statusLabel->setStyleSheet("color: #10b981;");
    }

private:
    LLMService *m_service;
    ContextManager *m_contextManager;
    QString m_currentContext;
    QString m_selectedText;

    // UI elements
    QVBoxLayout *m_messagesLayout{nullptr};
    QScrollArea *m_scrollArea{nullptr};
    ChatInputEdit *m_inputEdit{nullptr};
    QPushButton *m_sendButton{nullptr};
    QPushButton *m_cancelButton{nullptr};
    QPushButton *m_clearButton{nullptr};
    QLabel *m_statusLabel{nullptr};
    QLineEdit *m_apiKeyEdit{nullptr};
    QWidget *m_configWidget{nullptr};
    ChatMessageWidget *m_currentAssistantMessage{nullptr};

    // Context settings UI
    QWidget *m_contextSettingsWidget{nullptr};
    QPushButton *m_toggleContextSettingsButton{nullptr};
    QComboBox *m_contextModeCombo{nullptr};
    QSpinBox *m_surroundingPagesSpinBox{nullptr};
    QCheckBox *m_includeMetadataCheckBox{nullptr};
    QCheckBox *m_includeOutlineCheckBox{nullptr};
    QCheckBox *m_autoUpdateCheckBox{nullptr};
    QCheckBox *m_enableActionsCheckBox{nullptr};
    QLabel *m_contextInfoLabel{nullptr};

    // Quick action buttons
    QPushButton *m_summarizePageButton{nullptr};
    QPushButton *m_summarizeDocButton{nullptr};
    QPushButton *m_explainButton{nullptr};
    QPushButton *m_findEquationButton{nullptr};
    QPushButton *m_findFigureButton{nullptr};
    QPushButton *m_findSectionButton{nullptr};

    void setupUi()
    {
        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Configuration section (collapsible)
        m_configWidget = new QWidget(this);
        auto *configLayout = new QHBoxLayout(m_configWidget);
        configLayout->setContentsMargins(8, 8, 8, 8);

        auto *apiKeyLabel = new QLabel("Models Token:", m_configWidget);
        m_apiKeyEdit = new QLineEdit(m_configWidget);
        m_apiKeyEdit->setPlaceholderText("Enter GitHub Models token");
        m_apiKeyEdit->setEchoMode(QLineEdit::Password);

        auto *getTokenButton = new QPushButton("Get Token", m_configWidget);
        getTokenButton->setToolTip("Open GitHub Models to get your API token");
        connect(getTokenButton, &QPushButton::clicked, this, []() {
            QDesktopServices::openUrl(QUrl("https://github.com/marketplace/models"));
        });

        configLayout->addWidget(apiKeyLabel);
        configLayout->addWidget(m_apiKeyEdit, 1);
        configLayout->addWidget(getTokenButton);

        mainLayout->addWidget(m_configWidget);

        // Context settings section (collapsible)
        setupContextSettingsUi(mainLayout);

        // Separator
        auto *separator = new QFrame(this);
        separator->setFrameShape(QFrame::HLine);
        separator->setFrameShadow(QFrame::Sunken);
        mainLayout->addWidget(separator);

        // Quick actions bar
        setupQuickActionsUi(mainLayout);

        // Messages area
        m_scrollArea = new QScrollArea(this);
        m_scrollArea->setWidgetResizable(true);
        m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scrollArea->setFrameShape(QFrame::NoFrame);

        auto *messagesWidget = new QWidget(m_scrollArea);
        m_messagesLayout = new QVBoxLayout(messagesWidget);
        m_messagesLayout->setAlignment(Qt::AlignTop);
        m_messagesLayout->setSpacing(8);
        m_messagesLayout->setContentsMargins(8, 8, 8, 8);

        m_scrollArea->setWidget(messagesWidget);
        mainLayout->addWidget(m_scrollArea, 1);

        // Input area
        auto *inputWidget = new QWidget(this);
        auto *inputLayout = new QVBoxLayout(inputWidget);
        inputLayout->setContentsMargins(8, 8, 8, 8);
        inputLayout->setSpacing(4);

        // Status bar
        auto *statusLayout = new QHBoxLayout();
        m_statusLabel = new QLabel("Ready", inputWidget);
        m_statusLabel->setStyleSheet("color: #10b981; font-size: 11px;");

        m_contextInfoLabel = new QLabel("Context: ~0 tokens", inputWidget);
        m_contextInfoLabel->setStyleSheet("color: #6b7280; font-size: 11px;");

        m_clearButton = new QPushButton("Clear", inputWidget);
        m_clearButton->setMaximumWidth(60);

        statusLayout->addWidget(m_statusLabel);
        statusLayout->addStretch();
        statusLayout->addWidget(m_contextInfoLabel);
        statusLayout->addWidget(m_clearButton);

        inputLayout->addLayout(statusLayout);

        // Text input and buttons
        auto *inputRow = new QHBoxLayout();
        m_inputEdit = new ChatInputEdit(inputWidget);

        auto *buttonsLayout = new QVBoxLayout();
        m_sendButton = new QPushButton("Send", inputWidget);
        m_sendButton->setMinimumWidth(60);

        m_cancelButton = new QPushButton("Cancel", inputWidget);
        m_cancelButton->setMinimumWidth(60);
        m_cancelButton->setVisible(false);

        buttonsLayout->addWidget(m_sendButton);
        buttonsLayout->addWidget(m_cancelButton);
        buttonsLayout->addStretch();

        inputRow->addWidget(m_inputEdit, 1);
        inputRow->addLayout(buttonsLayout);

        inputLayout->addLayout(inputRow);
        mainLayout->addWidget(inputWidget);

        // Set minimum size
        setMinimumWidth(300);
    }

    void setupContextSettingsUi(QVBoxLayout *mainLayout)
    {
        // Toggle button for context settings
        m_toggleContextSettingsButton = new QPushButton("▼ Context Settings", this);
        m_toggleContextSettingsButton->setFlat(true);
        m_toggleContextSettingsButton->setStyleSheet(
            "QPushButton { text-align: left; padding: 4px 8px; color: #6b7280; }"
            "QPushButton:hover { background-color: palette(alternate-base); }");
        mainLayout->addWidget(m_toggleContextSettingsButton);

        // Context settings panel (hidden by default)
        m_contextSettingsWidget = new QWidget(this);
        m_contextSettingsWidget->setVisible(false);
        auto *settingsLayout = new QVBoxLayout(m_contextSettingsWidget);
        settingsLayout->setContentsMargins(8, 4, 8, 8);
        settingsLayout->setSpacing(8);

        // Context mode
        auto *modeLayout = new QHBoxLayout();
        modeLayout->addWidget(new QLabel("Context Mode:", m_contextSettingsWidget));
        m_contextModeCombo = new QComboBox(m_contextSettingsWidget);
        m_contextModeCombo->addItem("Current Page Only", 
            static_cast<int>(ContextManager::ContextMode::CurrentPage));
        m_contextModeCombo->addItem("Surrounding Pages", 
            static_cast<int>(ContextManager::ContextMode::SurroundingPages));
        m_contextModeCombo->addItem("Full Document", 
            static_cast<int>(ContextManager::ContextMode::FullDocument));
        m_contextModeCombo->addItem("Smart (Auto)", 
            static_cast<int>(ContextManager::ContextMode::SmartChunking));
        m_contextModeCombo->setCurrentIndex(1); // Default to Surrounding Pages
        modeLayout->addWidget(m_contextModeCombo, 1);
        settingsLayout->addLayout(modeLayout);

        // Surrounding pages count
        auto *pagesLayout = new QHBoxLayout();
        pagesLayout->addWidget(new QLabel("Surrounding Pages:", m_contextSettingsWidget));
        m_surroundingPagesSpinBox = new QSpinBox(m_contextSettingsWidget);
        m_surroundingPagesSpinBox->setRange(0, 10);
        m_surroundingPagesSpinBox->setValue(1);
        m_surroundingPagesSpinBox->setToolTip("Number of pages before and after current page to include");
        pagesLayout->addWidget(m_surroundingPagesSpinBox);
        pagesLayout->addStretch();
        settingsLayout->addLayout(pagesLayout);

        // Checkboxes
        m_includeMetadataCheckBox = new QCheckBox("Include document metadata", m_contextSettingsWidget);
        m_includeMetadataCheckBox->setChecked(true);
        m_includeMetadataCheckBox->setToolTip("Include title, author, and other PDF properties");
        settingsLayout->addWidget(m_includeMetadataCheckBox);

        m_includeOutlineCheckBox = new QCheckBox("Include table of contents", m_contextSettingsWidget);
        m_includeOutlineCheckBox->setChecked(true);
        m_includeOutlineCheckBox->setToolTip("Include document outline/bookmarks if available");
        settingsLayout->addWidget(m_includeOutlineCheckBox);

        m_autoUpdateCheckBox = new QCheckBox("Auto-update on page change", m_contextSettingsWidget);
        m_autoUpdateCheckBox->setChecked(true);
        m_autoUpdateCheckBox->setToolTip("Automatically update context when navigating pages");
        settingsLayout->addWidget(m_autoUpdateCheckBox);

        m_enableActionsCheckBox = new QCheckBox("Enable AI actions (navigation, search)", m_contextSettingsWidget);
        m_enableActionsCheckBox->setChecked(true);
        m_enableActionsCheckBox->setToolTip("Allow AI to navigate to pages, find equations, figures, etc.");
        settingsLayout->addWidget(m_enableActionsCheckBox);

        mainLayout->addWidget(m_contextSettingsWidget);
    }

    void setupQuickActionsUi(QVBoxLayout *mainLayout)
    {
        auto *quickActionsWidget = new QWidget(this);
        auto *quickActionsLayout = new QVBoxLayout(quickActionsWidget);
        quickActionsLayout->setContentsMargins(8, 4, 8, 4);
        quickActionsLayout->setSpacing(4);

        // First row: summarize and explain
        auto *row1 = new QHBoxLayout();
        row1->setSpacing(4);

        m_summarizePageButton = new QPushButton("Summarize Page", quickActionsWidget);
        m_summarizePageButton->setToolTip("Get a summary of the current page");
        m_summarizePageButton->setMaximumHeight(28);

        m_summarizeDocButton = new QPushButton("Summarize Doc", quickActionsWidget);
        m_summarizeDocButton->setToolTip("Get a summary of the entire document");
        m_summarizeDocButton->setMaximumHeight(28);

        m_explainButton = new QPushButton("Explain", quickActionsWidget);
        m_explainButton->setToolTip("Explain key concepts on this page");
        m_explainButton->setMaximumHeight(28);

        row1->addWidget(m_summarizePageButton);
        row1->addWidget(m_summarizeDocButton);
        row1->addWidget(m_explainButton);
        row1->addStretch();

        // Second row: navigation actions
        auto *row2 = new QHBoxLayout();
        row2->setSpacing(4);

        m_findEquationButton = new QPushButton("Find Equation", quickActionsWidget);
        m_findEquationButton->setToolTip("Navigate to a specific equation");
        m_findEquationButton->setMaximumHeight(28);

        m_findFigureButton = new QPushButton("Find Figure", quickActionsWidget);
        m_findFigureButton->setToolTip("Navigate to a specific figure");
        m_findFigureButton->setMaximumHeight(28);

        m_findSectionButton = new QPushButton("Go To Section", quickActionsWidget);
        m_findSectionButton->setToolTip("Navigate to a section by name");
        m_findSectionButton->setMaximumHeight(28);

        row2->addWidget(m_findEquationButton);
        row2->addWidget(m_findFigureButton);
        row2->addWidget(m_findSectionButton);
        row2->addStretch();

        quickActionsLayout->addLayout(row1);
        quickActionsLayout->addLayout(row2);

        mainLayout->addWidget(quickActionsWidget);
    }

    void connectSignals()
    {
        // Input signals
        connect(m_inputEdit, &ChatInputEdit::sendRequested, this,
                qOverload<>(&ChatWidget::sendMessage));
        connect(m_sendButton, &QPushButton::clicked, this,
                qOverload<>(&ChatWidget::sendMessage));
        connect(m_cancelButton, &QPushButton::clicked, m_service,
                &LLMService::cancelRequest);
        connect(m_clearButton, &QPushButton::clicked, this,
                &ChatWidget::clearConversation);
        connect(m_apiKeyEdit, &QLineEdit::editingFinished, this,
                &ChatWidget::onApiKeyChanged);

        // LLM service signals
        connect(m_service, &LLMService::streamingResponse, this,
                &ChatWidget::onStreamingResponse);
        connect(m_service, &LLMService::responseReceived, this,
                &ChatWidget::onResponseReceived);
        connect(m_service, &LLMService::errorOccurred, this,
                &ChatWidget::onError);
        connect(m_service, &LLMService::loadingChanged, this,
                &ChatWidget::onLoadingChanged);

        // Context settings signals
        connect(m_toggleContextSettingsButton, &QPushButton::clicked, this,
                &ChatWidget::toggleContextSettings);
        connect(m_contextModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &ChatWidget::onContextModeChanged);
        connect(m_surroundingPagesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ChatWidget::onSurroundingPagesChanged);
        connect(m_includeMetadataCheckBox, &QCheckBox::toggled,
                this, &ChatWidget::onIncludeMetadataChanged);
        connect(m_includeOutlineCheckBox, &QCheckBox::toggled,
                this, &ChatWidget::onIncludeOutlineChanged);
        connect(m_autoUpdateCheckBox, &QCheckBox::toggled,
                this, &ChatWidget::onAutoUpdateChanged);
        connect(m_enableActionsCheckBox, &QCheckBox::toggled,
                this, &ChatWidget::onEnableActionsChanged);

        // Context manager signals
        connect(m_contextManager, &ContextManager::contextUpdated,
                this, &ChatWidget::updateContextInfo);

        // Quick action buttons
        connect(m_summarizePageButton, &QPushButton::clicked,
                this, &ChatWidget::summarizePage);
        connect(m_summarizeDocButton, &QPushButton::clicked,
                this, &ChatWidget::summarizeDocument);
        connect(m_explainButton, &QPushButton::clicked,
                this, &ChatWidget::explainConcepts);
        connect(m_findEquationButton, &QPushButton::clicked,
                this, &ChatWidget::findEquationPrompt);
        connect(m_findFigureButton, &QPushButton::clicked,
                this, &ChatWidget::findFigurePrompt);
        connect(m_findSectionButton, &QPushButton::clicked,
                this, &ChatWidget::findSectionPrompt);

        // Action handler signals
        connect(m_service->actionHandler(), &ActionHandler::actionExecuted,
                this, &ChatWidget::onActionExecuted);
        connect(m_service->actionHandler(), &ActionHandler::navigationRequested,
                this, &ChatWidget::onNavigationRequested);
    }

    void updateConfigurationState()
    {
        bool configured = m_service->isConfigured();
        m_inputEdit->setEnabled(configured);
        m_sendButton->setEnabled(configured);
        m_summarizePageButton->setEnabled(configured);
        m_summarizeDocButton->setEnabled(configured);
        m_explainButton->setEnabled(configured);
        m_findEquationButton->setEnabled(configured);
        m_findFigureButton->setEnabled(configured);
        m_findSectionButton->setEnabled(configured);

        if (!configured)
        {
            m_statusLabel->setText("Please enter GitHub token");
            m_statusLabel->setStyleSheet("color: #f59e0b;");
        }
        else
        {
            m_statusLabel->setText("Ready");
            m_statusLabel->setStyleSheet("color: #10b981;");
        }
    }

    void addMessage(const QString &content, bool isUser)
    {
        auto *messageWidget = new ChatMessageWidget(content, isUser, this);
        m_messagesLayout->addWidget(messageWidget);
        scrollToBottom();
    }

    void scrollToBottom()
    {
        QTimer::singleShot(10, this, [this]() {
            m_scrollArea->verticalScrollBar()->setValue(
                m_scrollArea->verticalScrollBar()->maximum());
        });
    }
};