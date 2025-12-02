#pragma once

#include "GraphicsView.hpp"

#include <QColor>
#include <QHash>

struct Config
{
    QHash<QString, QString> shortcuts{};

    struct ui
    {
        QHash<QString, QColor> colors{};
        float link_hint_size{0.5f};
        float zoom_by{1.25f};
        float zoom{1.0};
        bool outline_shown{false};
        bool menubar_shown{true};
        bool panel_shown{true};
        bool auto_resize{false};
        bool link_boundary{false};
        bool jump_marker_shown{true};
        bool vscrollbar_shown{true};
        bool hscrollbar_shown{true};
        bool startup_tab{true};
        bool outline_as_side_panel{true};
        bool tabs_shown{true};
        bool auto_hide_tabs{false};
        int outline_panel_width{300};
        int selection_drag_threshold{50};
        QString window_title_format{"{} - dodo"};
        bool full_filepath_in_panel{false};
        QString initial_fit{"none"};
        bool page_nav_with_mouse{false};
    };

    struct rendering
    {
        float dpi{300};

        std::variant<float, QMap<QString, float>> dpr{};

        float inv_dpr{1.0f};
        bool icc_color_profile{true};
        int antialiasing_bits{8};
    };

    struct behavior
    {
        int undo_limit{25};
        int cache_pages{10};
        // bool auto_reload{true};
        bool invert_mode{false};
        bool open_last_visited{false};
        bool remember_last_visited{true};
        bool recent_files{true};
        int page_history_limit{5};
        int num_recent_files{10};
        int startpage_override{-1};
        GraphicsView::Mode initial_mode{
            GraphicsView::Mode::RegionSelection};
        QString synctex_editor_command{QString()};
    };

    ui ui{};
    rendering rendering{};
    behavior behavior{};
};
