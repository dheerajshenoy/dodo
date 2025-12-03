# dodo Configuration Guide

This document explains how to configure **dodo**, your PDF reader, using the `config.toml` file. All settings are optional; the defaults are included in the shipped `config.toml`.

---

## Table of Contents

1. [UI Settings](#ui-settings)
2. [Color Settings](#color-settings)
3. [Rendering Settings](#rendering-settings)
4. [Behavior](#behavior-settings)
5. [Keyboard Shortcuts](#keyboard-shortcuts)

---

## UI Settings `[ui]`

Control how dodo's UI and panels appear:

| Setting                    | Type     | Default       | Description                                             |
| -------------------------- | -------- | ------------- | ------------------------------------------------------- |
| `panel`                    | `bool`   | `true`        | Show or hide the side panel on startup.                 |
| `fullscreen`               | `bool`   | `false`       | Launch dodo in fullscreen mode.                         |
| `zoom_level`               | `float`  | `1.0`         | Default zoom factor for pages.                          |
| `link_boundary`            | `bool`   | `false`       | Show bounding boxes around links.                       |
| `hscrollbar`               | `bool`   | `false`       | Show horizontal scrollbar.                              |
| `vscrollbar`               | `bool`   | `false`       | Show vertical scrollbar.                                |
| `full_file_path_in_panel`  | `bool`   | `false`       | Display the full file path in the side panel.           |
| `menubar`                  | `bool`   | `false`       | Show the menu bar.                                      |
| `window_title`             | `string` | `"{} - dodo"` | Window title format; `{}` replaced with file base name. |
| `jump_marker`              | `bool`   | `true`        | Show jump markers when navigating links.                |
| `selection_drag_threshold` | `int`    | `100`         | Minimum drag distance to start a selection.             |
| `tabs`                     | `bool`   | `true`        | Show tabs for multiple documents.                       |
| `auto_hide_tabs`           | `bool`   | `true`        | Hide tabs if only one file is open.                     |
| `startup_tab`              | `bool`   | `true`        | Show the startup tab on launch.                         |
| `link_hint_size`           | `float`  | `0.25`        | Size of link hints (in pixels).                         |
| `outline`                  | `bool`   | `true`        | Enable the document outline panel.                      |
| `outline_as_side_panel`    | `bool`   | `true`        | Show outline in side panel instead of popup.            |
| `outline_panel_position`   | `string` | `"left"`      | "left" or "right" position of the outline panel.        |
| `outline_panel_width`      | `int`    | `300`         | Width of the outline panel (if enabled).                |

---

## Color Settings `[colors]`

Colors are specified in ARGB format (`#AARRGGBB`):

| Setting        | Default       | Description                           |
| -------------- | ------------- | ------------------------------------- |
| `accent`       | `#3daee9FF`   | Accent color for UI elements.         |
| `background`   | `#00000000`   | Default background color.             |
| `search_match` | `#55500033`   | Highlight color for search matches.   |
| `search_index` | `#55FF0055`   | Highlight for current search result.  |
| `link_hint_bg` | `#FFFF0044`   | Background for link hints.            |
| `link_hint_fg` | `#FF0000FF`   | Foreground (text) for link hints.     |
| `selection`    | `#330000FF`   | Color for selection rectangle.        |
| `highlight`    | `#55FF0055`   | Highlight color for text annotations. |
| `jump_marker`  | `#FFFF0000`   | Marker color for jump destinations.   |
| `annot_rect`   | `#55FF5500`   | Color for annotation rectangles.      |

---

## Rendering Settings `[rendering]`

Adjust how PDFs are rendered:

| Setting             | Type           | Default | Description                                                                                                  |
| ------------------- | -------------- | ------- | ------------------------------------------------------------------------------------------------------------ |
| `dpi`               | `float`        | `300.0` | PDF rendering DPI.                                                                                           |
| `dpr`               | `float or map` | `1.0`   | Device pixel ratio for high-DPI displays. Can be a single float or a map of display names to DPR values.    |
| `cache_pages`       | `int`          | `0`     | Number of pages to keep in RAM for faster navigation. Set to 0 to disable caching.                          |
| `antialiasing_bits` | `int`          | `8`     | Quality of antialiasing (4=good, 8=high).                                                                    |
| `icc_color_profile` | `bool`         | `true`  | Enable ICC color profile support.                                                                            |

### DPR Configuration Examples

Single value for all displays:
```toml
dpr = 1.25
```

Per-display values:
```toml
dpr = { "eDP-1" = 1.25, "DP-5" = 1.0, "DP-7" = 1.0 }
```

---

## Behavior Settings `[behavior]`

Control default modes and UI behavior:

| Setting                  | Type     | Default                                  | Description                                                                                        |
| ------------------------ | -------- | ---------------------------------------- | -------------------------------------------------------------------------------------------------- |
| `initial_mode`           | `string` | `"text_select"`                          | Mode at startup: "region_select", "annot_rect", "annot_select", "text_select", "text_highlight".   |
| `remember_last_visited`  | `bool`   | `true`                                   | Go to last visited page when reopening a file.                                                     |
| `page_history`           | `int`    | `100`                                    | Number of visited pages to remember per session.                                                   |
| `confirm_on_quit`        | `bool`   | `true`                                   | Prompt before quitting dodo.                                                                       |
| `auto_resize`            | `bool`   | `false`                                  | Auto-resize pages to fit the window.                                                               |
| `zoom_factor`            | `float`  | `1.25`                                   | Factor used for zoom in/out.                                                                       |
| `initial_fit`            | `string` | `"width"`                                | Initial page fit mode: "none", "width", "height", "window".                                        |
| `invert_mode`            | `bool`   | `true`                                   | Start in inverted color mode.                                                                      |
| `synctex_editor_command` | `string` | `"kitty -e nvim --remote-silent +%l %f"` | Command to open synced TeX editor. `%l` = line number, `%f` = file path.                           |
| `auto_reload`            | `bool`   | `true`                                   | Reload the document if file changes externally.                                                    |
| `recent_files`           | `bool`   | `true`                                   | Keep a list of recently opened files.                                                              |
| `num_recent_files`       | `int`    | `25`                                     | Maximum number of recent files (negative value for unlimited).                                     |
| `undo_limit`             | `int`    | `25`                                     | Maximum number of undo operations to keep in history.                                              |

---

## Keyboard Shortcuts `[keybindings]`

Customize keybindings in the `[keybindings]` section. Below is a complete list of available actions:

### File Operations

| Action       | Default Shortcut | Description             |
| ------------ | ---------------- | ----------------------- |
| `open_file`  | `o`              | Open a PDF file.        |
| `close_file` | `Ctrl+w`         | Close the current file. |
| `save`       | `Ctrl+S`         | Save annotations.       |

### Navigation

| Action            | Default Shortcut | Description                |
| ----------------- | ---------------- | -------------------------- |
| `scroll_left`     | `h`              | Scroll left.               |
| `scroll_down`     | `j`              | Scroll down.               |
| `scroll_up`       | `k`              | Scroll up.                 |
| `scroll_right`    | `l`              | Scroll right.              |
| `next_page`       | `Shift+J`        | Go to next page.           |
| `prev_page`       | `Shift+K`        | Go to previous page.       |
| `first_page`      | `g,g`            | Jump to first page.        |
| `last_page`       | `Shift+G`        | Jump to last page.         |
| `goto_page`       | `b`              | Go to a specific page.     |
| `prev_location`   | `Ctrl+o`         | Jump back in history.      |
| `outline`         | `t`              | Toggle outline panel.      |

### Link Hints

| Action            | Default Shortcut | Description       |
| ----------------- | ---------------- | ----------------- |
| `link_hint_visit` | `f`              | Jump to a link.   |
| `link_hint_copy`  | `Shift+F`        | Copy link URL.    |

### Search

| Action            | Default Shortcut | Description                       |
| ----------------- | ---------------- | --------------------------------- |
| `search`          | `/`              | Open search input.                |
| `search_this_page`| `?`              | Limit search to current page.     |
| `search_next`     | `n`              | Go to next search result.         |
| `search_prev`     | `Shift+N`        | Go to previous search result.     |

### Zoom & Fit

| Action        | Default Shortcut | Description                      |
| ------------- | ---------------- | -------------------------------- |
| `zoom_in`     | `=`              | Zoom in.                         |
| `zoom_out`    | `-`              | Zoom out.                        |
| `zoom_reset`  | `0`              | Reset zoom to default.           |
| `fit_height`  | `Ctrl+Shift+H`   | Fit page to window height.       |
| `fit_width`   | `Ctrl+Shift+W`   | Fit page to window width.        |
| `fit_window`  | `Ctrl+Shift+=`   | Fit page to window.              |
| `auto_resize` | `Ctrl+Shift+R`   | Toggle auto-resize mode.         |

### Annotations & Selection

| Action                             | Default Shortcut | Description                              |
| ---------------------------------- | ---------------- | ---------------------------------------- |
| `text_highlight_current_selection` | `1`              | Highlight currently selected text.       |
| `text_highlight_mode`              | `Alt+1`          | Switch to text highlight mode.           |
| `annot_rect_mode`                  | `Alt+2`          | Switch to rectangle annotation mode.     |
| `annot_edit_mode`                  | `Alt+3`          | Switch to annotation edit/select mode.   |
| `region_select_mode`               | `1`              | Switch to region select mode.            |
| `cancel_selection`                 | `escape`         | Cancel current selection.                |
| `yank`                             | `y`              | Copy selected text to clipboard.         |
| `select_all`                       | `Ctrl+A`         | Select all text/annotations.             |
| `delete`                           | `d`              | Delete selected annotation.              |
| `list_text_highlights`             | `]`              | List all text highlights.                |
| `undo`                             | `u`              | Undo last annotation operation.          |
| `redo`                             | `Ctrl+R`         | Redo last undone operation.              |

### View & UI Toggles

| Action           | Default Shortcut | Description              |
| ---------------- | ---------------- | ------------------------ |
| `fullscreen`     | `F11`            | Toggle fullscreen.       |
| `invert_color`   | `i`              | Toggle inverted mode.    |
| `toggle_menubar` | `Ctrl+Shift+M`   | Toggle menu bar.         |
| `toggle_panel`   | `Ctrl+Shift+B`   | Toggle side panel.       |
| `toggle_tabs`    | `Ctrl+Shift+T`   | Toggle tabs visibility.  |

### Tabs

| Action     | Default Shortcut | Description              |
| ---------- | ---------------- | ------------------------ |
| `tab_next` | `Ctrl+.`         | Go to next tab.          |
| `tab_prev` | `Ctrl+,`         | Go to previous tab.      |
| `tab1`     | `Alt+1`          | Go to tab 1.             |
| `tab2`     | `Alt+2`          | Go to tab 2.             |
| `tab3`     | `Alt+3`          | Go to tab 3.             |
| `tab4`     | `Alt+4`          | Go to tab 4.             |
| `tab5`     | `Alt+5`          | Go to tab 5.             |
| `tab6`     | `Alt+6`          | Go to tab 6.             |
| `tab7`     | `Alt+7`          | Go to tab 7.             |
| `tab8`     | `Alt+8`          | Go to tab 8.             |
| `tab9`     | `Alt+9`          | Go to tab 9.             |

### Rotation

| Action            | Default Shortcut | Description                  |
| ----------------- | ---------------- | ---------------------------- |
| `rotate_clock`    | `>`              | Rotate page clockwise.       |
| `rotate_anticlock`| `<`              | Rotate page counter-clockwise.|

### Miscellaneous

| Action            | Default Shortcut | Description              |
| ----------------- | ---------------- | ------------------------ |
| `file_properties` | `Ctrl+Shift+F`   | Show file properties.    |
| `command`         | `:`              | Open command input.      |
| `about`           | `F12`            | Show about dialog.       |

---

## Notes

* Colors are in **ARGB format**; transparency can be controlled with the alpha channel (`AA`).
* DPI and DPR should match your display for crisp rendering.
* Selection and annotation modes are controlled via `initial_mode` and can be toggled via keyboard shortcuts.
* Always restart dodo after modifying `config.toml` to apply changes.
