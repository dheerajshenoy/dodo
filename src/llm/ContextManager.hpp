#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QTimer>
#include <QMutex>
#include <functional>

// Forward declarations to avoid circular includes
class Model;
class DocumentView;
struct fz_outline;

// ContextManager provides enhanced AI context awareness for PDF documents
// Features:
// - Multi-page context (current page + surrounding pages)
// - Full document context extraction (for smaller documents)
// - Document metadata inclusion
// - Automatic context updates on page change
// - Context chunking for large documents
class ContextManager : public QObject
{
    Q_OBJECT

public:
    // Context mode determines how much of the document is included
    enum class ContextMode
    {
        CurrentPage,      // Only the current page
        SurroundingPages, // Current page + N pages before/after
        FullDocument,     // Entire document (warning: may be large)
        SmartChunking     // Intelligent chunking based on document size
    };

    struct ContextSettings
    {
        ContextMode mode{ContextMode::SurroundingPages};
        int surroundingPages{1};        // Pages before/after current page
        int maxContextLength{16000};    // Max characters in context
        bool includeMetadata{true};     // Include PDF properties
        bool includeOutline{true};      // Include table of contents
        bool includePageNumbers{true};  // Add page number markers
        bool autoUpdate{true};          // Auto-update on page change
    };

    struct DocumentMetadata
    {
        QString title;
        QString author;
        QString subject;
        QString keywords;
        QString creator;
        QString producer;
        QString creationDate;
        QString modificationDate;
        int totalPages{0};
        QString filePath;
        QString fileName;
    };

    explicit ContextManager(QObject *parent = nullptr);
    ~ContextManager() = default;

    // Set the document view to extract context from
    void setDocumentView(DocumentView *docView);

    // Get the current compiled context string
    QString getContext() const;

    // Force refresh the context
    void refreshContext();

    // Settings
    void setSettings(const ContextSettings &settings);
    ContextSettings settings() const { return m_settings; }

    // Individual setting modifiers
    void setContextMode(ContextMode mode);
    void setSurroundingPages(int pages);
    void setMaxContextLength(int maxLength);
    void setIncludeMetadata(bool include);
    void setIncludeOutline(bool include);
    void setAutoUpdate(bool autoUpdate);

    // Get metadata
    DocumentMetadata documentMetadata() const { return m_metadata; }

    // Extract text from a specific page (0-indexed)
    QString getPageText(int pageNo) const;

    // Get all pages text (cached)
    QHash<int, QString> getAllPagesText() const { return m_pageTextCache; }

    // Check if full document is cached
    bool isFullDocumentCached() const { return m_fullDocumentCached; }

    // Get estimated token count (rough approximation)
    int estimatedTokenCount() const;

signals:
    // Emitted when context is updated
    void contextUpdated(const QString &newContext);

    // Emitted when caching progress changes
    void cachingProgress(int currentPage, int totalPages);

    // Emitted when full document caching completes
    void fullDocumentCached();

public slots:
    // Called when page changes
    void onPageChanged(int newPageNo);

    // Cache entire document in background
    void cacheFullDocument();

    // Clear all caches
    void clearCache();

private:
    void extractMetadata();
    void extractOutline();
    QString buildContextString() const;
    QString getTextForPageRange(int startPage, int endPage) const;
    QString formatMetadataSection() const;
    QString formatOutlineSection() const;
    QString truncateToMaxLength(const QString &text) const;

    DocumentView *m_docView{nullptr};
    Model *m_model{nullptr};
    ContextSettings m_settings;
    DocumentMetadata m_metadata;
    QString m_outlineText;
    mutable QString m_cachedContext;

    // Page text cache (page number -> text)
    mutable QHash<int, QString> m_pageTextCache;
    mutable QMutex m_cacheMutex;

    int m_currentPage{0};
    bool m_fullDocumentCached{false};
    mutable bool m_contextDirty{true};

    // Timer for debouncing context updates
    QTimer *m_updateDebounceTimer{nullptr};
};