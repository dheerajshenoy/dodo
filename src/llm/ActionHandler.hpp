#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <functional>
#include <map>

class DocumentView;
class Model;

// ActionHandler enables AI to perform actions in the PDF reader
// It parses AI responses for action commands and executes them
//
// The AI can embed actions in its responses using a simple format:
// [[ACTION:action_name:param1:param2:...]]
//
// Examples:
// [[ACTION:goto_page:15]]
// [[ACTION:search:neural networks]]
// [[ACTION:goto_outline:Chapter 3]]
class ActionHandler : public QObject
{
    Q_OBJECT

public:
    // Action result returned after executing an action
    struct ActionResult
    {
        bool success{false};
        QString message;
        QString actionName;
        QStringList params;
    };

    // Action definition
    struct ActionDefinition
    {
        QString name;
        QString description;
        QStringList parameterNames;
        QStringList parameterDescriptions;
        std::function<ActionResult(const QStringList &)> handler;
    };

    explicit ActionHandler(QObject *parent = nullptr);
    ~ActionHandler() = default;

    // Set the document view to control
    void setDocumentView(DocumentView *docView);

    // Parse AI response and extract any actions
    QList<ActionResult> parseAndExecuteActions(const QString &aiResponse);

    // Get the cleaned response (with action tags removed)
    QString getCleanedResponse(const QString &aiResponse) const;

    // Get available actions as a prompt for the AI
    QString getActionsPrompt() const;

    // Get available actions as JSON schema (for function calling)
    QJsonArray getActionsSchema() const;

    // Check if handler is ready (has document view)
    bool isReady() const { return m_docView != nullptr; }

    // Search document for text and return matching pages
    QList<int> searchDocument(const QString &query) const;

    // Find page containing outline item
    int findOutlineItem(const QString &title) const;

    // Find equation by number (e.g., "3.2", "eq. 5")
    int findEquation(const QString &equationRef) const;

    // Find figure by number
    int findFigure(const QString &figureRef) const;

    // Find table by number
    int findTable(const QString &tableRef) const;

    // Find section by name or number
    int findSection(const QString &sectionRef) const;

signals:
    // Emitted when an action is executed
    void actionExecuted(const ActionResult &result);

    // Emitted when navigation occurs
    void navigationRequested(int pageNo, const QString &reason);

    // Emitted to show a message to the user
    void showMessage(const QString &message);

private:
    void registerActions();

    // Action implementations
    ActionResult actionGotoPage(const QStringList &params);
    ActionResult actionSearch(const QStringList &params);
    ActionResult actionGotoOutline(const QStringList &params);
    ActionResult actionFindEquation(const QStringList &params);
    ActionResult actionFindFigure(const QStringList &params);
    ActionResult actionFindTable(const QStringList &params);
    ActionResult actionFindSection(const QStringList &params);
    ActionResult actionNextPage(const QStringList &params);
    ActionResult actionPrevPage(const QStringList &params);
    ActionResult actionFirstPage(const QStringList &params);
    ActionResult actionLastPage(const QStringList &params);
    ActionResult actionHighlightText(const QStringList &params);
    ActionResult actionCopyText(const QStringList &params);
    ActionResult actionZoomIn(const QStringList &params);
    ActionResult actionZoomOut(const QStringList &params);

    // Helper to search page text for patterns
    struct SearchMatch
    {
        int page;
        int position;
        QString context;
    };
    QList<SearchMatch> searchPages(const QString &pattern, bool regex = false) const;

    // Helper to traverse outline
    int findInOutline(struct fz_outline *node, const QString &title, 
                      bool fuzzy = true) const;

    DocumentView *m_docView{nullptr};
    Model *m_model{nullptr};
    std::map<QString, ActionDefinition> m_actions;

    // Regex for parsing action tags
    QRegularExpression m_actionRegex;
};

// ============================================================================
// Implementation
// ============================================================================

#include "../DocumentView.hpp"
#include "../Model.hpp"
#include <mupdf/fitz.h>
#include <QClipboard>
#include <QGuiApplication>

inline ActionHandler::ActionHandler(QObject *parent)
    : QObject(parent)
    , m_actionRegex(R"(\[\[ACTION:([^:\]]+)(?::([^\]]*))?\]\])")
{
    registerActions();
}

inline void ActionHandler::setDocumentView(DocumentView *docView)
{
    m_docView = docView;
    m_model = docView ? docView->model() : nullptr;
}

inline void ActionHandler::registerActions()
{
    // Navigation actions
    m_actions["goto_page"] = {
        "goto_page",
        "Navigate to a specific page number",
        {"page_number"},
        {"The page number to navigate to (1-indexed)"},
        [this](const QStringList &p) { return actionGotoPage(p); }
    };

    m_actions["search"] = {
        "search",
        "Search for text in the document and go to the first match",
        {"query"},
        {"The text to search for"},
        [this](const QStringList &p) { return actionSearch(p); }
    };

    m_actions["goto_outline"] = {
        "goto_outline",
        "Navigate to a section from the table of contents by title",
        {"section_title"},
        {"The title of the section to navigate to"},
        [this](const QStringList &p) { return actionGotoOutline(p); }
    };

    m_actions["find_equation"] = {
        "find_equation",
        "Find and navigate to an equation by its number",
        {"equation_ref"},
        {"The equation reference (e.g., '3.2', 'eq. 5', 'equation 12')"},
        [this](const QStringList &p) { return actionFindEquation(p); }
    };

    m_actions["find_figure"] = {
        "find_figure",
        "Find and navigate to a figure by its number",
        {"figure_ref"},
        {"The figure reference (e.g., '3', 'Figure 5', 'fig. 12')"},
        [this](const QStringList &p) { return actionFindFigure(p); }
    };

    m_actions["find_table"] = {
        "find_table",
        "Find and navigate to a table by its number",
        {"table_ref"},
        {"The table reference (e.g., '1', 'Table 3', 'tab. 5')"},
        [this](const QStringList &p) { return actionFindTable(p); }
    };

    m_actions["find_section"] = {
        "find_section",
        "Find and navigate to a section by name or number",
        {"section_ref"},
        {"The section reference (e.g., 'Introduction', 'Section 3.1', '2.2')"},
        [this](const QStringList &p) { return actionFindSection(p); }
    };

    m_actions["next_page"] = {
        "next_page",
        "Go to the next page",
        {},
        {},
        [this](const QStringList &p) { return actionNextPage(p); }
    };

    m_actions["prev_page"] = {
        "prev_page",
        "Go to the previous page",
        {},
        {},
        [this](const QStringList &p) { return actionPrevPage(p); }
    };

    m_actions["first_page"] = {
        "first_page",
        "Go to the first page of the document",
        {},
        {},
        [this](const QStringList &p) { return actionFirstPage(p); }
    };

    m_actions["last_page"] = {
        "last_page",
        "Go to the last page of the document",
        {},
        {},
        [this](const QStringList &p) { return actionLastPage(p); }
    };

    m_actions["zoom_in"] = {
        "zoom_in",
        "Zoom in on the document",
        {},
        {},
        [this](const QStringList &p) { return actionZoomIn(p); }
    };

    m_actions["zoom_out"] = {
        "zoom_out",
        "Zoom out on the document",
        {},
        {},
        [this](const QStringList &p) { return actionZoomOut(p); }
    };

    m_actions["copy_text"] = {
        "copy_text",
        "Copy specified text to clipboard",
        {"text"},
        {"The text to copy to clipboard"},
        [this](const QStringList &p) { return actionCopyText(p); }
    };
}

inline QString ActionHandler::getActionsPrompt() const
{
    QString prompt = "You can perform actions in the PDF reader by including action tags in your response.\n";
    prompt += "Format: [[ACTION:action_name:parameter]]\n\n";
    prompt += "Available actions:\n";

    for (const auto &[name, action] : m_actions)
    {
        prompt += QString("- %1: %2\n").arg(action.name, action.description);
        if (!action.parameterNames.isEmpty())
        {
            prompt += QString("  Parameters: %1\n").arg(action.parameterNames.join(", "));
        }
    }

    prompt += "\nExamples:\n";
    prompt += "- To go to page 15: [[ACTION:goto_page:15]]\n";
    prompt += "- To find equation 3.2: [[ACTION:find_equation:3.2]]\n";
    prompt += "- To search for 'neural network': [[ACTION:search:neural network]]\n";
    prompt += "- To go to a section: [[ACTION:find_section:Introduction]]\n";
    prompt += "\nAlways explain what you're doing before or after the action tag.\n";

    return prompt;
}

inline QJsonArray ActionHandler::getActionsSchema() const
{
    QJsonArray schema;

    for (const auto &[name, action] : m_actions)
    {
        QJsonObject actionObj;
        actionObj["name"] = action.name;
        actionObj["description"] = action.description;

        QJsonObject parameters;
        parameters["type"] = "object";

        QJsonObject properties;
        QJsonArray required;

        for (int i = 0; i < action.parameterNames.size(); ++i)
        {
            QJsonObject param;
            param["type"] = "string";
            param["description"] = action.parameterDescriptions.value(i);
            properties[action.parameterNames[i]] = param;
            required.append(action.parameterNames[i]);
        }

        parameters["properties"] = properties;
        parameters["required"] = required;
        actionObj["parameters"] = parameters;

        schema.append(actionObj);
    }

    return schema;
}

inline QList<ActionHandler::ActionResult> ActionHandler::parseAndExecuteActions(
    const QString &aiResponse)
{
    QList<ActionResult> results;

    if (!m_docView)
    {
        return results;
    }

    QRegularExpressionMatchIterator it = m_actionRegex.globalMatch(aiResponse);

    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        QString actionName = match.captured(1).toLower().trimmed();
        QString paramsStr = match.captured(2);

        QStringList params;
        if (!paramsStr.isEmpty())
        {
            // Split by colon, but handle cases where the parameter might contain colons
            params = paramsStr.split(':');
        }

        if (m_actions.contains(actionName))
        {
            ActionResult result = m_actions[actionName].handler(params);
            result.actionName = actionName;
            result.params = params;
            results.append(result);
            emit actionExecuted(result);
        }
        else
        {
            ActionResult result;
            result.success = false;
            result.actionName = actionName;
            result.params = params;
            result.message = QString("Unknown action: %1").arg(actionName);
            results.append(result);
        }
    }

    return results;
}

inline QString ActionHandler::getCleanedResponse(const QString &aiResponse) const
{
    QString cleaned = aiResponse;
    cleaned.remove(m_actionRegex);
    return cleaned.trimmed();
}

inline QList<ActionHandler::SearchMatch> ActionHandler::searchPages(
    const QString &pattern, bool regex) const
{
    QList<SearchMatch> matches;

    if (!m_model)
        return matches;

    int totalPages = m_model->numPages();
    QRegularExpression re;

    if (regex)
    {
        re = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
    }

    for (int page = 0; page < totalPages; ++page)
    {
        QString pageText = m_model->getPageText(page);

        if (regex)
        {
            QRegularExpressionMatch match = re.match(pageText);
            if (match.hasMatch())
            {
                SearchMatch sm;
                sm.page = page;
                sm.position = match.capturedStart();

                // Get context around the match
                int start = qMax(0, sm.position - 50);
                int end = qMin(pageText.length(), sm.position + match.capturedLength() + 50);
                sm.context = pageText.mid(start, end - start);

                matches.append(sm);
            }
        }
        else
        {
            int pos = pageText.indexOf(pattern, 0, Qt::CaseInsensitive);
            if (pos >= 0)
            {
                SearchMatch sm;
                sm.page = page;
                sm.position = pos;

                // Get context around the match
                int start = qMax(0, pos - 50);
                int end = qMin(pageText.length(), pos + pattern.length() + 50);
                sm.context = pageText.mid(start, end - start);

                matches.append(sm);
            }
        }
    }

    return matches;
}

inline QList<int> ActionHandler::searchDocument(const QString &query) const
{
    QList<int> pages;
    auto matches = searchPages(query, false);
    for (const auto &match : matches)
    {
        if (!pages.contains(match.page))
            pages.append(match.page);
    }
    return pages;
}

inline int ActionHandler::findInOutline(fz_outline *node, const QString &title,
                                        bool fuzzy) const
{
    while (node)
    {
        QString nodeTitle = QString::fromUtf8(node->title ? node->title : "");

        bool match = false;
        if (fuzzy)
        {
            // Fuzzy match: check if title contains the search term
            match = nodeTitle.contains(title, Qt::CaseInsensitive);
        }
        else
        {
            match = (nodeTitle.compare(title, Qt::CaseInsensitive) == 0);
        }

        if (match)
        {
            // Get page number from the outline destination
            if (node->page.page >= 0)
                return node->page.page;
        }

        // Search children
        if (node->down)
        {
            int result = findInOutline(node->down, title, fuzzy);
            if (result >= 0)
                return result;
        }

        node = node->next;
    }

    return -1;
}

inline int ActionHandler::findOutlineItem(const QString &title) const
{
    if (!m_model)
        return -1;

    fz_outline *outline = m_model->getOutline();
    if (!outline)
        return -1;

    // Try exact match first, then fuzzy
    int page = findInOutline(outline, title, false);
    if (page < 0)
        page = findInOutline(outline, title, true);

    return page;
}

inline int ActionHandler::findEquation(const QString &equationRef) const
{
    if (!m_model)
        return -1;

    // Extract the equation number
    QString numStr = equationRef;
    numStr.remove(QRegularExpression(R"(^(eq\.?|equation)\s*)", 
                                      QRegularExpression::CaseInsensitiveOption));
    numStr = numStr.trimmed();

    // Build search patterns for common equation formats
    QStringList patterns = {
        QString(R"(\(\s*%1\s*\))").arg(QRegularExpression::escape(numStr)),  // (3.2)
        QString(R"(Equation\s+%1\b)").arg(QRegularExpression::escape(numStr)),  // Equation 3.2
        QString(R"(Eq\.?\s*%1\b)").arg(QRegularExpression::escape(numStr)),  // Eq. 3.2 or Eq 3.2
        QString(R"(\[\s*%1\s*\])").arg(QRegularExpression::escape(numStr)),  // [3.2]
    };

    for (const QString &pattern : patterns)
    {
        auto matches = searchPages(pattern, true);
        if (!matches.isEmpty())
        {
            return matches.first().page;
        }
    }

    return -1;
}

inline int ActionHandler::findFigure(const QString &figureRef) const
{
    if (!m_model)
        return -1;

    QString numStr = figureRef;
    numStr.remove(QRegularExpression(R"(^(fig\.?|figure)\s*)", 
                                      QRegularExpression::CaseInsensitiveOption));
    numStr = numStr.trimmed();

    QStringList patterns = {
        QString(R"(Figure\s+%1\b)").arg(QRegularExpression::escape(numStr)),
        QString(R"(Fig\.?\s*%1\b)").arg(QRegularExpression::escape(numStr)),
    };

    for (const QString &pattern : patterns)
    {
        auto matches = searchPages(pattern, true);
        if (!matches.isEmpty())
        {
            return matches.first().page;
        }
    }

    return -1;
}

inline int ActionHandler::findTable(const QString &tableRef) const
{
    if (!m_model)
        return -1;

    QString numStr = tableRef;
    numStr.remove(QRegularExpression(R"(^(tab\.?|table)\s*)", 
                                      QRegularExpression::CaseInsensitiveOption));
    numStr = numStr.trimmed();

    QStringList patterns = {
        QString(R"(Table\s+%1\b)").arg(QRegularExpression::escape(numStr)),
        QString(R"(Tab\.?\s*%1\b)").arg(QRegularExpression::escape(numStr)),
    };

    for (const QString &pattern : patterns)
    {
        auto matches = searchPages(pattern, true);
        if (!matches.isEmpty())
        {
            return matches.first().page;
        }
    }

    return -1;
}

inline int ActionHandler::findSection(const QString &sectionRef) const
{
    if (!m_model)
        return -1;

    // First try outline
    int page = findOutlineItem(sectionRef);
    if (page >= 0)
        return page;

    // Try searching for section headers
    QString cleanRef = sectionRef;
    cleanRef.remove(QRegularExpression(R"(^(section|sec\.?)\s*)", 
                                        QRegularExpression::CaseInsensitiveOption));
    cleanRef = cleanRef.trimmed();

    QStringList patterns = {
        QString(R"(^\s*%1[\.\s])").arg(QRegularExpression::escape(cleanRef)),  // "3.1 Title"
        QString(R"(Section\s+%1\b)").arg(QRegularExpression::escape(cleanRef)),
        QString(R"(^\s*\d+\.?\s+%1)").arg(QRegularExpression::escape(cleanRef)),  // "3. Introduction"
    };

    for (const QString &pattern : patterns)
    {
        auto matches = searchPages(pattern, true);
        if (!matches.isEmpty())
        {
            return matches.first().page;
        }
    }

    // Last resort: simple text search
    auto matches = searchPages(cleanRef, false);
    if (!matches.isEmpty())
    {
        return matches.first().page;
    }

    return -1;
}

// ============================================================================
// Action Implementations
// ============================================================================

inline ActionHandler::ActionResult ActionHandler::actionGotoPage(const QStringList &params)
{
    ActionResult result;

    if (params.isEmpty())
    {
        result.success = false;
        result.message = "Page number required";
        return result;
    }

    bool ok;
    int pageNo = params[0].toInt(&ok);

    if (!ok)
    {
        result.success = false;
        result.message = QString("Invalid page number: %1").arg(params[0]);
        return result;
    }

    int totalPages = m_model->numPages();

    if (pageNo < 1 || pageNo > totalPages)
    {
        result.success = false;
        result.message = QString("Page %1 out of range (1-%2)").arg(pageNo).arg(totalPages);
        return result;
    }

    // GotoPage expects 1-indexed page numbers
    m_docView->GotoPage(pageNo);
    emit navigationRequested(pageNo - 1, QString("Navigated to page %1").arg(pageNo));

    result.success = true;
    result.message = QString("Navigated to page %1").arg(pageNo);
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionSearch(const QStringList &params)
{
    ActionResult result;

    if (params.isEmpty())
    {
        result.success = false;
        result.message = "Search query required";
        return result;
    }

    QString query = params.join(":");  // Rejoin in case query had colons

    QList<int> pages = searchDocument(query);

    if (pages.isEmpty())
    {
        result.success = false;
        result.message = QString("No matches found for: %1").arg(query);
        return result;
    }

    // Go to first match (GotoPage expects 1-indexed)
    m_docView->GotoPage(pages.first() + 1);
    emit navigationRequested(pages.first(), 
        QString("Found '%1' on page %2").arg(query).arg(pages.first() + 1));

    result.success = true;
    result.message = QString("Found '%1' on page %2 (and %3 other page(s))")
                         .arg(query)
                         .arg(pages.first() + 1)
                         .arg(pages.size() - 1);
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionGotoOutline(const QStringList &params)
{
    ActionResult result;

    if (params.isEmpty())
    {
        result.success = false;
        result.message = "Section title required";
        return result;
    }

    QString title = params.join(":");

    int page = findOutlineItem(title);

    if (page < 0)
    {
        result.success = false;
        result.message = QString("Section not found: %1").arg(title);
        return result;
    }

    // GotoPage expects 1-indexed page numbers
    m_docView->GotoPage(page + 1);
    emit navigationRequested(page, QString("Navigated to section: %1").arg(title));

    result.success = true;
    result.message = QString("Navigated to '%1' (page %2)").arg(title).arg(page + 1);
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionFindEquation(const QStringList &params)
{
    ActionResult result;

    if (params.isEmpty())
    {
        result.success = false;
        result.message = "Equation reference required";
        return result;
    }

    QString ref = params.join(":");

    int page = findEquation(ref);

    if (page < 0)
    {
        result.success = false;
        result.message = QString("Equation not found: %1").arg(ref);
        return result;
    }

    // GotoPage expects 1-indexed page numbers
    m_docView->GotoPage(page + 1);
    emit navigationRequested(page, QString("Found equation %1").arg(ref));

    result.success = true;
    result.message = QString("Found equation %1 on page %2").arg(ref).arg(page + 1);
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionFindFigure(const QStringList &params)
{
    ActionResult result;

    if (params.isEmpty())
    {
        result.success = false;
        result.message = "Figure reference required";
        return result;
    }

    QString ref = params.join(":");

    int page = findFigure(ref);

    if (page < 0)
    {
        result.success = false;
        result.message = QString("Figure not found: %1").arg(ref);
        return result;
    }

    // GotoPage expects 1-indexed page numbers
    m_docView->GotoPage(page + 1);
    emit navigationRequested(page, QString("Found figure %1").arg(ref));

    result.success = true;
    result.message = QString("Found figure %1 on page %2").arg(ref).arg(page + 1);
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionFindTable(const QStringList &params)
{
    ActionResult result;

    if (params.isEmpty())
    {
        result.success = false;
        result.message = "Table reference required";
        return result;
    }

    QString ref = params.join(":");

    int page = findTable(ref);

    if (page < 0)
    {
        result.success = false;
        result.message = QString("Table not found: %1").arg(ref);
        return result;
    }

    // GotoPage expects 1-indexed page numbers
    m_docView->GotoPage(page + 1);
    emit navigationRequested(page, QString("Found table %1").arg(ref));

    result.success = true;
    result.message = QString("Found table %1 on page %2").arg(ref).arg(page + 1);
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionFindSection(const QStringList &params)
{
    ActionResult result;

    if (params.isEmpty())
    {
        result.success = false;
        result.message = "Section reference required";
        return result;
    }

    QString ref = params.join(":");

    int page = findSection(ref);

    if (page < 0)
    {
        result.success = false;
        result.message = QString("Section not found: %1").arg(ref);
        return result;
    }

    // GotoPage expects 1-indexed page numbers
    m_docView->GotoPage(page + 1);
    emit navigationRequested(page, QString("Found section: %1").arg(ref));

    result.success = true;
    result.message = QString("Found section '%1' on page %2").arg(ref).arg(page + 1);
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionNextPage(const QStringList &params)
{
    Q_UNUSED(params);
    ActionResult result;

    int currentPage = m_docView->pageNo();
    int totalPages = m_model->numPages();

    if (currentPage >= totalPages - 1)
    {
        result.success = false;
        result.message = "Already on the last page";
        return result;
    }

    m_docView->NextPage();
    emit navigationRequested(currentPage + 1, "Moved to next page");

    result.success = true;
    result.message = QString("Moved to page %1").arg(currentPage + 2);
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionPrevPage(const QStringList &params)
{
    Q_UNUSED(params);
    ActionResult result;

    int currentPage = m_docView->pageNo();

    if (currentPage <= 0)
    {
        result.success = false;
        result.message = "Already on the first page";
        return result;
    }

    m_docView->PrevPage();
    emit navigationRequested(currentPage - 1, "Moved to previous page");

    result.success = true;
    result.message = QString("Moved to page %1").arg(currentPage);
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionFirstPage(const QStringList &params)
{
    Q_UNUSED(params);
    ActionResult result;

    m_docView->FirstPage();
    emit navigationRequested(0, "Moved to first page");

    result.success = true;
    result.message = "Moved to first page";
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionLastPage(const QStringList &params)
{
    Q_UNUSED(params);
    ActionResult result;

    m_docView->LastPage();
    int lastPage = m_model->numPages();
    emit navigationRequested(lastPage - 1, "Moved to last page");

    result.success = true;
    result.message = QString("Moved to last page (%1)").arg(lastPage);
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionZoomIn(const QStringList &params)
{
    Q_UNUSED(params);
    ActionResult result;

    m_docView->ZoomIn();

    result.success = true;
    result.message = "Zoomed in";
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionZoomOut(const QStringList &params)
{
    Q_UNUSED(params);
    ActionResult result;

    m_docView->ZoomOut();

    result.success = true;
    result.message = "Zoomed out";
    return result;
}

inline ActionHandler::ActionResult ActionHandler::actionCopyText(const QStringList &params)
{
    ActionResult result;

    if (params.isEmpty())
    {
        result.success = false;
        result.message = "Text to copy required";
        return result;
    }

    QString text = params.join(":");
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);

    result.success = true;
    result.message = "Text copied to clipboard";
    return result;
}