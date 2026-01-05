#pragma once

#include "GraphicsView.hpp"

#include <QColor>
#include <QHash>

struct Config
{
    QHash<QString, QString> shortcuts{};

    struct ui
    {
        struct window
        {
            bool fullscreen{false};
            bool menubar{true};
            bool panel{true};
            bool startup_tab{true};
            bool full_file_path_in_panel{false};
            QString title_format{"{} - dodo"};
        } window{};

        struct layout
        {
            QString mode{"top_to_bottom"};
            QString initial_fit{"none"};
            bool auto_resize{false};
        } layout{};

        struct zoom
        {
            float level{1.0f};
            float factor{1.25f};
        } zoom{};

        struct selection
        {
            int drag_threshold{50};
        } selection{};

        struct scrollbars
        {
            bool horizontal{true};
            bool vertical{true};
            bool search_hits{true};
        } scrollbars{};

        struct markers
        {
            bool jump_marker{true};
        } markers{};

        struct links
        {
            bool boundary{false};
        } links{};

        struct link_hints
        {
            float size{0.5f};
        } link_hints{};

        struct tabs
        {
            bool visible{true};
            bool auto_hide{false};
            bool closable{true};
            bool movable{true};
            QString elide_mode{"right"};
            QString bar_position{"top"};
        } tabs{};

        struct outline
        {
            bool visible{false};
            bool as_side_panel{true};
            QString type{"side_panel"};
            QString panel_position{"left"};
            int panel_width{300};
        } outline{};

        struct highlight_search
        {
            bool visible{false};
            bool as_side_panel{false};
            QString type{"dialog"};
            QString panel_position{"right"};
            int panel_width{300};
        } highlight_search{};

        QHash<QString, QColor> colors{};
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
        bool auto_reload{true};
        bool config_auto_reload{true};
        bool invert_mode{false};
        bool open_last_visited{false};
        bool remember_last_visited{true};
        bool recent_files{true};
        int page_history_limit{5};
        int num_recent_files{10};
        int startpage_override{-1};
        GraphicsView::Mode initial_mode{GraphicsView::Mode::RegionSelection};
        QString synctex_editor_command{QString()};
    };

    ui ui{};
    rendering rendering{};
    behavior behavior{};
};
