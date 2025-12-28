#pragma once

#include <QPointF>
#include <algorithm>
#include <vector>

extern "C"
{
#include <mupdf/fitz.h>
}

static inline float
min4(float a, float b, float c, float d) noexcept
{
    return std::min({a, b, c, d});
}

static inline float
max4(float a, float b, float c, float d) noexcept
{
    return std::max({a, b, c, d});
}

static inline float
quad_top(const fz_quad &q) noexcept
{
    return min4(q.ul.y, q.ur.y, q.ll.y, q.lr.y);
}

static inline float
quad_bottom(const fz_quad &q) noexcept
{
    return max4(q.ul.y, q.ur.y, q.ll.y, q.lr.y);
}

static inline float
quad_left(const fz_quad &q) noexcept
{
    return min4(q.ul.x, q.ur.x, q.ll.x, q.lr.x);
}

static inline float
quad_right(const fz_quad &q) noexcept
{
    return max4(q.ul.x, q.ur.x, q.ll.x, q.lr.x);
}

std::vector<fz_rect>
merge_selected_character_rects(
    const std::vector<fz_rect> &selected_character_rects) noexcept;

bool
is_consequtive(const fz_rect &rect1, const fz_rect &rect2) noexcept;

fz_rect
bound_rects(const std::vector<fz_rect> &rects) noexcept;

std::vector<fz_quad>
quads_from_rects(const std::vector<fz_rect> &rects) noexcept;

fz_quad
quad_from_rect(const fz_rect &r) noexcept;

std::vector<fz_quad>
merged_quads_from_quads(const std::vector<fz_quad> &quads) noexcept;

bool
same_line(const fz_quad &a, const fz_quad &b) noexcept;

std::vector<fz_quad>
merge_quads_by_line(const std::vector<fz_quad> &input) noexcept;

static inline float
quad_y_center(const fz_quad &q)
{
    return (q.ul.y + q.ll.y + q.ur.y + q.lr.y) * 0.25f;
}

void
selectionHelper(const QPointF &selectionStart, const QPointF &selectionEnd,
                fz_point &a, fz_point &b, const fz_matrix &transform,
                float dpr) noexcept;
