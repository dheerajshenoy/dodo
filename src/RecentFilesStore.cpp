#include "RecentFilesStore.hpp"

#include <QFile>
#include <QJsonValue>
#include <algorithm>

namespace
{
QDateTime
parseTimestamp(const QJsonValue &value)
{
    if (value.isString())
    {
        QDateTime ts = QDateTime::fromString(value.toString(), Qt::ISODate);
        if (ts.isValid())
            return ts;
    }
    if (value.isDouble())
    {
        return QDateTime::fromMSecsSinceEpoch(
            static_cast<qint64>(value.toDouble()));
    }
    return {};
}
} // namespace

RecentFilesStore::RecentFilesStore(const QString &filePath)
    : m_file_path(filePath)
{
}

void
RecentFilesStore::setFilePath(const QString &filePath) noexcept
{
    m_file_path = filePath;
}

bool
RecentFilesStore::load() noexcept
{
    m_entries.clear();

    if (m_file_path.isEmpty())
        return false;

    if (!QFile::exists(m_file_path))
        return true;

    QFile file(m_file_path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError)
        return false;

    QJsonArray entriesArray;
    if (doc.isObject())
    {
        entriesArray = doc.object().value("entries").toArray();
    }
    else if (doc.isArray())
    {
        entriesArray = doc.array();
    }

    for (const QJsonValue &value : entriesArray)
    {
        if (!value.isObject())
            continue;
        RecentFileEntry entry = parseEntry(value.toObject());
        if (entry.file_path.isEmpty())
            continue;
        m_entries.push_back(entry);
    }

    sortByAccessedDesc();
    return true;
}

bool
RecentFilesStore::save() const noexcept
{
    if (m_file_path.isEmpty())
        return false;

    QJsonArray entriesArray;
    for (const RecentFileEntry &entry : m_entries)
        entriesArray.append(serializeEntry(entry));

    QJsonObject root;
    root.insert("version", 1);
    root.insert("entries", entriesArray);

    QSaveFile file(m_file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

const QVector<RecentFileEntry> &
RecentFilesStore::entries() const noexcept
{
    return m_entries;
}

void
RecentFilesStore::setEntries(QVector<RecentFileEntry> entries) noexcept
{
    m_entries = std::move(entries);
    sortByAccessedDesc();
}

void
RecentFilesStore::upsert(const QString &filePath, int pageNumber,
                         const QDateTime &accessed) noexcept
{
    for (RecentFileEntry &entry : m_entries)
    {
        if (entry.file_path == filePath)
        {
            entry.page_number   = pageNumber;
            entry.last_accessed = accessed;
            sortByAccessedDesc();
            return;
        }
    }

    RecentFileEntry entry;
    entry.file_path     = filePath;
    entry.page_number   = pageNumber;
    entry.last_accessed = accessed;
    m_entries.push_back(entry);
    sortByAccessedDesc();
}

void
RecentFilesStore::trim(int maxEntries) noexcept
{
    if (maxEntries < 0)
        return;

    if (m_entries.size() > maxEntries)
        m_entries.resize(maxEntries);
}

void
RecentFilesStore::sortByAccessedDesc() noexcept
{
    std::sort(m_entries.begin(), m_entries.end(),
              [](const RecentFileEntry &a, const RecentFileEntry &b)
    { return a.last_accessed > b.last_accessed; });
}

RecentFileEntry
RecentFilesStore::parseEntry(const QJsonObject &obj) noexcept
{
    RecentFileEntry entry;
    entry.file_path     = obj.value("file_path").toString();
    entry.page_number   = obj.value("page_number").toVariant().toInt();
    entry.last_accessed = parseTimestamp(obj.value("last_accessed"));
    if (!entry.last_accessed.isValid())
        entry.last_accessed = QDateTime::fromMSecsSinceEpoch(0);
    return entry;
}

QJsonObject
RecentFilesStore::serializeEntry(const RecentFileEntry &entry)
{
    QJsonObject obj;
    obj.insert("file_path", entry.file_path);
    obj.insert("page_number", entry.page_number);
    obj.insert("last_accessed", entry.last_accessed.toString(Qt::ISODate));
    return obj;
}
