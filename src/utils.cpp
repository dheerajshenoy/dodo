#include "utils.hpp"

fz_rect
bound_rects(const std::vector<fz_rect> &rects) noexcept
{
    // find the bounding box of some rects

    fz_rect res = rects[0];

    float average_y0 = 0.0f;
    float average_y1 = 0.0f;

    for (auto rect : rects)
    {
        if (res.x1 < rect.x1)
        {
            res.x1 = rect.x1;
        }
        if (res.x0 > rect.x0)
        {
            res.x0 = rect.x0;
        }

        average_y0 += rect.y0;
        average_y1 += rect.y1;
    }

    average_y0 /= rects.size();
    average_y1 /= rects.size();

    res.y0 = average_y0;
    res.y1 = average_y1;

    return res;
}

bool
is_consequtive(const fz_rect &rect1, const fz_rect &rect2) noexcept
{

    float xdist  = abs(rect1.x1 - rect2.x1);
    float ydist1 = abs(rect1.y0 - rect2.y0);
    float ydist2 = abs(rect1.y1 - rect2.y1);
    float ydist  = std::min(ydist1, ydist2);

    float rect1_width   = rect1.x1 - rect1.x0;
    float rect2_width   = rect2.x1 - rect2.x0;
    float average_width = (rect1_width + rect2_width) / 2.0f;

    float rect1_height   = rect1.y1 - rect1.y0;
    float rect2_height   = rect2.y1 - rect2.y0;
    float average_height = (rect1_height + rect2_height) / 2.0f;

    if (xdist < 3 * average_width && ydist < 2 * average_height)
    {
        return true;
    }

    return false;
}

std::vector<fz_rect>
merge_selected_character_rects(
    const std::vector<fz_rect> &selected_character_rects) noexcept
{
    /*
        This function merges the bounding boxes of all selected characters into
       large line chunks.
    */
    std::vector<fz_rect> resulting_rects;

    if (selected_character_rects.size() == 0)
    {
        return {};
    }

    std::vector<fz_rect> line_rects;

    fz_rect last_rect = selected_character_rects[0];
    line_rects.push_back(selected_character_rects[0]);

    for (size_t i = 1; i < selected_character_rects.size(); i++)
    {
        if (is_consequtive(last_rect, selected_character_rects[i]))
        {
            last_rect = selected_character_rects[i];
            line_rects.push_back(selected_character_rects[i]);
        }
        else
        {
            fz_rect bounding_rect = bound_rects(line_rects);
            resulting_rects.push_back(bounding_rect);
            line_rects.clear();
            last_rect = selected_character_rects[i];
            line_rects.push_back(selected_character_rects[i]);
        }
    }

    if (line_rects.size() > 0)
    {
        fz_rect bounding_rect = bound_rects(line_rects);
        resulting_rects.push_back(bounding_rect);
    }

    // avoid overlapping rects
    for (size_t i = 0; i < resulting_rects.size() - 1; i++)
    {
        // we don't need to do this across columns of document
        float height = std::abs(resulting_rects[i].y1 - resulting_rects[i].y0);
        if (std::abs(resulting_rects[i + 1].y0 - resulting_rects[i].y0)
            < (0.5 * height))
        {
            continue;
        }
        if ((resulting_rects[i + 1].x0 < resulting_rects[i].x1))
        {
            resulting_rects[i + 1].y0 = resulting_rects[i].y1;
        }
    }

    return resulting_rects;
}

bool
same_line(const fz_quad &a, const fz_quad &b) noexcept
{
    float a_top = quad_top(a);
    float a_bot = quad_bottom(a);
    float b_top = quad_top(b);
    float b_bot = quad_bottom(b);

    float overlap = std::min(a_bot, b_bot) - std::max(a_top, b_top);
    float height  = std::min(a_bot - a_top, b_bot - b_top);

    // Require meaningful vertical overlap
    return overlap > 0.5f * height;
}

std::vector<fz_quad>
merged_quads_from_quads(const std::vector<fz_quad> &quads) noexcept
{
    if (quads.empty())
        return {};

    std::vector<fz_quad> sorted = quads;

    // Sort top-to-bottom, left-to-right
    std::sort(sorted.begin(), sorted.end(),
              [](const fz_quad &a, const fz_quad &b)
    {
        float dy = quad_top(a) - quad_top(b);
        if (std::fabs(dy) > 1.0f)
            return dy < 0.0f;
        return quad_left(a) < quad_left(b);
    });

    std::vector<fz_quad> result;
    fz_quad current = sorted.front();

    for (size_t i = 1; i < sorted.size(); ++i)
    {
        const fz_quad &q = sorted[i];

        if (same_line(current, q))
        {
            // Merge horizontally
            float left   = std::min(quad_left(current), quad_left(q));
            float right  = std::max(quad_right(current), quad_right(q));
            float top    = std::min(quad_top(current), quad_top(q));
            float bottom = std::max(quad_bottom(current), quad_bottom(q));

            current.ul = {left, top};
            current.ur = {right, top};
            current.ll = {left, bottom};
            current.lr = {right, bottom};
        }
        else
        {
            result.push_back(current);
            current = q;
        }
    }

    result.push_back(current);
    return result;
}

fz_quad
quad_from_rect(const fz_rect &r) noexcept
{
    fz_quad q;
    q.ul = fz_make_point(r.x0, r.y0);
    q.ur = fz_make_point(r.x1, r.y0);
    q.ll = fz_make_point(r.x0, r.y1);
    q.lr = fz_make_point(r.x1, r.y1);
    return q;
}

std::vector<fz_quad>
quads_from_rects(const std::vector<fz_rect> &rects) noexcept
{
    std::vector<fz_quad> res;
    for (auto rect : rects)
    {
        res.push_back(quad_from_rect(rect));
    }
    return res;
}

std::vector<fz_quad>
merge_quads_by_line(const std::vector<fz_quad> &input) noexcept
{
    if (input.empty())
        return {};

    constexpr float Y_EPS = 2.0f; // tweak if needed

    struct Line
    {
        float y;
        float xmin, xmax;
        float top, bottom;
    };

    std::vector<Line> lines;

    for (const auto &q : input)
    {
        float yc = quad_y_center(q);

        float xmin = std::min({q.ul.x, q.ll.x, q.ur.x, q.lr.x});
        float xmax = std::max({q.ul.x, q.ll.x, q.ur.x, q.lr.x});
        float top  = std::min(q.ul.y, q.ur.y);
        float bot  = std::max(q.ll.y, q.lr.y);

        bool merged = false;

        for (auto &line : lines)
        {
            if (std::abs(line.y - yc) < Y_EPS)
            {
                line.xmin   = std::min(line.xmin, xmin);
                line.xmax   = std::max(line.xmax, xmax);
                line.top    = std::min(line.top, top);
                line.bottom = std::max(line.bottom, bot);
                merged      = true;
                break;
            }
        }

        if (!merged)
        {
            lines.push_back({yc, xmin, xmax, top, bot});
        }
    }

    std::vector<fz_quad> out;
    out.reserve(lines.size());

    for (const auto &l : lines)
    {
        fz_quad q{};
        q.ul = {l.xmin, l.top};
        q.ur = {l.xmax, l.top};
        q.ll = {l.xmin, l.bottom};
        q.lr = {l.xmax, l.bottom};
        out.push_back(q);
    }

    return out;
}

fz_quad
getQuadForSubstring(fz_stext_line *line, int start, int len)
{
    fz_rect rect;
    int i = 0;

    for (fz_stext_char *ch = line->first_char; ch; ch = ch->next, ++i)
    {
        if (i >= start && i < (start + len))
        {
            rect = fz_union_rect(rect, fz_rect_from_quad(ch->quad));
        }
    }
    return fz_quad_from_rect(rect);
}
