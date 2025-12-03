#include "ContextManager.hpp"
#include "../DocumentView.hpp"
#include "../Model.hpp"

#include <mupdf/fitz.h>

ContextManager::ContextManager(QObject *parent)
    : QObject(parent)
    , m_updateDebounceTimer(new QTimer(this))
{
    m_updateDebounceTimer->setSingleShot(true);
    m_updateDebounceTimer->setInterval(100); // 100ms debounce

    connect(m_updateDebounceTimer, &QTimer::timeout, this, [this]() {
        refreshContext();
    });
}

void ContextManager::setDocumentView(DocumentView *docView)
{
    if (m_docView == docView)
        return;

    // Disconnect from previous document view
    if (m_docView)
    {
        disconnect(m_docView, &DocumentView::pageNumberChanged,
                   this, &ContextManager::onPageChanged);
    }

    m_docView = docView;
    clearCache();

    if (m_docView)
    {
        // Get model from document view
        m_model = m_docView->model();
        m_currentPage = m_docView->pageNo();

        // Extract metadata
        extractMetadata();
        extractOutline();

        // Connect to page change signal
        connect(m_docView, &DocumentView::pageNumberChanged,
                this, &ContextManager::onPageChanged);

        // Initial context build
        refreshContext();
    }
    else
    {
        m_model = nullptr;
    }
}

QString ContextManager::getContext() const
{
    if (m_contextDirty)
    {
        m_cachedContext = buildContextString();
        m_contextDirty = false;
    }
    return m_cachedContext;
}

void ContextManager::refreshContext()
{
    m_cachedContext = buildContextString();
    m_contextDirty = false;
    emit contextUpdated(m_cachedContext);
}

void ContextManager::setSettings(const ContextSettings &settings)
{
    m_settings = settings;
    m_contextDirty = true;
    if (m_settings.autoUpdate)
        refreshContext();
}

void ContextManager::setContextMode(ContextMode mode)
{
    m_settings.mode = mode;
    m_contextDirty = true;
    if (m_settings.autoUpdate)
        m_updateDebounceTimer->start();
}

void ContextManager::setSurroundingPages(int pages)
{
    m_settings.surroundingPages = qMax(0, pages);
    m_contextDirty = true;
    if (m_settings.autoUpdate)
        m_updateDebounceTimer->start();
}

void ContextManager::setMaxContextLength(int maxLength)
{
    m_settings.maxContextLength = qMax(1000, maxLength);
    m_contextDirty = true;
    if (m_settings.autoUpdate)
        m_updateDebounceTimer->start();
}

void ContextManager::setIncludeMetadata(bool include)
{
    m_settings.includeMetadata = include;
    m_contextDirty = true;
    if (m_settings.autoUpdate)
        m_updateDebounceTimer->start();
}

void ContextManager::setIncludeOutline(bool include)
{
    m_settings.includeOutline = include;
    m_contextDirty = true;
    if (m_settings.autoUpdate)
        m_updateDebounceTimer->start();
}

void ContextManager::setAutoUpdate(bool autoUpdate)
{
    m_settings.autoUpdate = autoUpdate;
}

void ContextManager::onPageChanged(int newPageNo)
{
    if (m_currentPage == newPageNo)
        return;

    m_currentPage = newPageNo;
    m_contextDirty = true;

    if (m_settings.autoUpdate)
    {
        m_updateDebounceTimer->start();
    }
}

void ContextManager::clearCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_pageTextCache.clear();
    m_fullDocumentCached = false;
    m_contextDirty = true;
    m_metadata = DocumentMetadata();
    m_outlineText.clear();
    m_cachedContext.clear();
}

QString ContextManager::getPageText(int pageNo) const
{
    QMutexLocker locker(&m_cacheMutex);

    // Check cache first
    if (m_pageTextCache.contains(pageNo))
        return m_pageTextCache.value(pageNo);

    // Extract and cache
    if (!m_model)
        return QString();

    QString text = m_model->getPageText(pageNo);
    m_pageTextCache.insert(pageNo, text);
    return text;
}

void ContextManager::cacheFullDocument()
{
    if (!m_model || m_fullDocumentCached)
        return;

    int totalPages = m_model->numPages();

    for (int i = 0; i < totalPages; ++i)
    {
        getPageText(i);
        emit cachingProgress(i + 1, totalPages);
    }

    m_fullDocumentCached = true;
    emit fullDocumentCached();
}

void ContextManager::extractMetadata()
{
    if (!m_model || !m_docView)
        return;

    m_metadata.totalPages = m_model->numPages();
    m_metadata.filePath = m_docView->fileName();

    // Extract file name from path
    int lastSlash = m_metadata.filePath.lastIndexOf('/');
    if (lastSlash == -1)
        lastSlash = m_metadata.filePath.lastIndexOf('\\');

    m_metadata.fileName = (lastSlash >= 0)
                              ? m_metadata.filePath.mid(lastSlash + 1)
                              : m_metadata.filePath;

    // Get PDF properties from model
    QList<QPair<QString, QString>> props = m_model->extractPDFProperties();
    for (const auto &prop : props)
    {
        QString key = prop.first.toLower();
        QString value = prop.second;

        if (key == "title")
            m_metadata.title = value;
        else if (key == "author")
            m_metadata.author = value;
        else if (key == "subject")
            m_metadata.subject = value;
        else if (key == "keywords")
            m_metadata.keywords = value;
        else if (key == "creator")
            m_metadata.creator = value;
        else if (key == "producer")
            m_metadata.producer = value;
        else if (key == "creationdate" || key == "creation date")
            m_metadata.creationDate = value;
        else if (key == "moddate" || key == "modification date")
            m_metadata.modificationDate = value;
    }
}

void ContextManager::extractOutline()
{
    if (!m_model)
        return;

    fz_outline *outline = m_model->getOutline();
    if (!outline)
        return;

    // Recursive lambda to traverse outline
    std::function<void(fz_outline *, int)> traverseOutline =
        [&](fz_outline *node, int depth) {
            while (node)
            {
                QString indent = QString("  ").repeated(depth);
                QString title = QString::fromUtf8(node->title ? node->title : "");

                if (!title.isEmpty())
                {
                    m_outlineText += indent + "- " + title + "\n";
                }

                if (node->down)
                    traverseOutline(node->down, depth + 1);

                node = node->next;
            }
        };

    m_outlineText = "Table of Contents:\n";
    traverseOutline(outline, 0);
}

QString ContextManager::formatMetadataSection() const
{
    if (!m_settings.includeMetadata)
        return QString();

    QStringList parts;
    parts << "=== Document Information ===";

    if (!m_metadata.fileName.isEmpty())
        parts << QString("File: %1").arg(m_metadata.fileName);
    if (!m_metadata.title.isEmpty())
        parts << QString("Title: %1").arg(m_metadata.title);
    if (!m_metadata.author.isEmpty())
        parts << QString("Author: %1").arg(m_metadata.author);
    if (!m_metadata.subject.isEmpty())
        parts << QString("Subject: %1").arg(m_metadata.subject);
    if (!m_metadata.keywords.isEmpty())
        parts << QString("Keywords: %1").arg(m_metadata.keywords);
    if (m_metadata.totalPages > 0)
        parts << QString("Total Pages: %1").arg(m_metadata.totalPages);

    parts << QString("Current Page: %1 of %2")
                 .arg(m_currentPage + 1)
                 .arg(m_metadata.totalPages);

    return parts.join("\n") + "\n\n";
}

QString ContextManager::formatOutlineSection() const
{
    if (!m_settings.includeOutline || m_outlineText.isEmpty())
        return QString();

    return "=== " + m_outlineText + "\n";
}

QString ContextManager::getTextForPageRange(int startPage, int endPage) const
{
    if (!m_model)
        return QString();

    int totalPages = m_model->numPages();
    startPage = qMax(0, startPage);
    endPage = qMin(totalPages - 1, endPage);

    QString result;
    for (int page = startPage; page <= endPage; ++page)
    {
        QString pageText = getPageText(page);

        if (m_settings.includePageNumbers && !pageText.isEmpty())
        {
            result += QString("\n--- Page %1 ---\n").arg(page + 1);
        }

        result += pageText;

        if (page < endPage)
            result += "\n";
    }

    return result;
}

QString ContextManager::buildContextString() const
{
    if (!m_model)
        return QString();

    QString context;

    // Add metadata section
    context += formatMetadataSection();

    // Add outline section (truncated if too long)
    QString outline = formatOutlineSection();
    if (outline.length() < m_settings.maxContextLength / 4)
    {
        context += outline;
    }

    // Add page content based on mode
    QString pageContent;
    int totalPages = m_model->numPages();

    switch (m_settings.mode)
    {
    case ContextMode::CurrentPage:
        pageContent = getTextForPageRange(m_currentPage, m_currentPage);
        break;

    case ContextMode::SurroundingPages:
    {
        int startPage = m_currentPage - m_settings.surroundingPages;
        int endPage = m_currentPage + m_settings.surroundingPages;
        pageContent = getTextForPageRange(startPage, endPage);
        break;
    }

    case ContextMode::FullDocument:
        pageContent = getTextForPageRange(0, totalPages - 1);
        break;

    case ContextMode::SmartChunking:
    {
        // For small documents (< 20 pages), include everything
        // For larger documents, use surrounding pages with more context
        if (totalPages <= 20)
        {
            pageContent = getTextForPageRange(0, totalPages - 1);
        }
        else
        {
            // Include more surrounding pages for larger documents
            int extraPages = qMin(5, totalPages / 10);
            int startPage = m_currentPage - m_settings.surroundingPages - extraPages;
            int endPage = m_currentPage + m_settings.surroundingPages + extraPages;
            pageContent = getTextForPageRange(startPage, endPage);
        }
        break;
    }
    }

    context += "=== Document Content ===\n";
    context += pageContent;

    // Truncate if needed
    return truncateToMaxLength(context);
}

QString ContextManager::truncateToMaxLength(const QString &text) const
{
    if (text.length() <= m_settings.maxContextLength)
        return text;

    // Try to truncate at a word boundary
    QString truncated = text.left(m_settings.maxContextLength);
    int lastSpace = truncated.lastIndexOf(' ');

    if (lastSpace > m_settings.maxContextLength * 0.8)
    {
        truncated = truncated.left(lastSpace);
    }

    return truncated + "\n\n[... content truncated due to length ...]";
}

int ContextManager::estimatedTokenCount() const
{
    // Rough approximation: ~4 characters per token for English text
    return m_cachedContext.length() / 4;
}