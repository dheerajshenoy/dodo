#include "MarkManager.hpp"

void
MarkManager::addMark(const QString &key, const Mark &mark) noexcept
{
    if (key.isEmpty() || mark.plocation.pageno < 0)
        return;
    m_marks[key] = mark;
}

MarkManager::Mark
MarkManager::getMark(const QString &key) const noexcept
{
    auto it = m_marks.find(key);
    if (it == m_marks.end())
        return {};
    return it->second;
}

std::vector<MarkManager::Mark>
MarkManager::marks() const noexcept
{
    std::vector<Mark> marks;
    marks.reserve(m_marks.size());
    for (auto &entry : m_marks)
        marks.emplace_back(entry.second);
    return marks;
}
