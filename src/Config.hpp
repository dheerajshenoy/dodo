#pragma once

#include <QColor>
#include <QMap>

struct Config
{
    QMap<QString, QColor> colors{};
    QMap<QString, QString> shortcuts_map{};

    float dpi{300};
    float dpr{1.0f};
    float inv_dpr{1.0f};
    float zoom_by{1.25f};
    float zoom{1.0};
    float link_hint_size{0.5f};

    int startpage_override{-1};
    int page_history_limit{5};
    int selection_drag_threshold{50};
    int cache_pages{10};
    int antialiasing_bits{8};
    int num_recent_files{10};
    int outline_panel_width{300};


    // bool auto_reload{true};
    bool menubar_shown{true};
    bool panel_shown{true};
    bool icc_color_profile{true};
    bool invert_mode{false};
    bool page_nav_with_mouse{false};
    bool jump_marker_shown{true};
    bool full_filepath_in_panel{false};
    bool open_last_visited{false};
    bool auto_resize{false};
    bool link_boundary{false};
    bool remember_last_visited{true};
    bool vscrollbar_shown{true};
    bool hscrollbar_shown{true};
    bool startup_tab{true};
    bool recent_files{true};
    bool outline_as_side_panel{true};
    bool auto_hide_tabs{false};
    bool tabs_shown{true};

    QString initial_fit{"none"};
    QString window_title_format{"{} - dodo"};
    QString synctex_editor_command{QString()};
};
