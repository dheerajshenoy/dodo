#pragma once

#include "DocumentView.hpp"

#include <unordered_map>

class MarkManager
{
public:
    struct Mark
    {
        uint32_t tabId;
        DocumentView::PageLocation plocation;
    };

    inline bool hasMark(const QString &key) const noexcept
    {
        return m_marks.contains(key);
    }

    inline bool removeMark(const QString &key) noexcept
    {
        return m_marks.erase(key);
    }

    void addMark(const QString &key, const Mark &mark) noexcept;
    Mark getMark(const QString &key) const noexcept;

    std::vector<Mark> marks() const noexcept;

private:
    std::unordered_map<QString, Mark> m_marks;
};
