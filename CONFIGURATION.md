# dodo Configuration Guide

This document explains how to configure **dodo**, your PDF reader, using the `config.toml` file. All settings are optional; the defaults are included in the shipped `config.toml`.

---

## Table of Contents

1. [Appearance Settings](#appearance-settings)
2. [Color Settings](#color-settings)
3. [Rendering Settings](#rendering-settings)
4. [Navigation & Behavior](#navigation--behavior)
5. [Keyboard Shortcuts](#keyboard-shortcuts)

---

## Appearance Settings

Control how dodo’s UI and panels appear:

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
| `outline_as_side_panel`    | `bool`   | `false`       | Show outline as popup instead of side panel.            |
| `outline_panel_position`   | `string` | `"left"`      | "left" or "right" position of the outline panel.        |
| `outline_panel_width`      | `int`    | `300`         | Width of the outline panel (if enabled).                |

---

## Color Settings

Colors are specified in ARGB format (`#AARRGGBB`):

| Setting        | Default     | Description                           |
| -------------- | ----------- | ------------------------------------- |
| `accent`       | `#3daee9FF` | Accent color for UI elements.         |
| `background`   | `#00000000` | Default background color.             |
| `search_match` | `#55500033` | Highlight color for search matches.   |
| `search_index` | `#55FF0055` | Highlight for current search result.  |
| `link_hint_bg` | `#FFFF0044` | Background for link hints.            |
| `link_hint_fg` | `#FF0000FF` | Foreground (text) for link hints.     |
| `selection`    | `#330000FF` | Color for selection rectangle.        |
| `highlight`    | `#55FF0055` | Highlight color for text annotations. |
| `jump_marker`  | `#FFFF0000` | Marker color for jump destinations.   |
| `annot_rect`   | `#55FF5500` | Color for annotation rectangles.      |

---

## Rendering Settings

Adjust how PDFs are rendered:

| Setting             | Type    | Default | Description                                           |
| ------------------- | ------- | ------- | ----------------------------------------------------- |
| `dpi`               | `float` | `300.0` | PDF rendering DPI.                                    |
| `dpr`               | `float` | `1.25`  | Device pixel ratio for high-DPI displays.             |
| `cache_pages`       | `int`   | `5`     | Number of pages to keep in RAM for faster navigation. |
| `antialiasing_bits` | `int`   | `8`     | Quality of antialiasing (4=good, 8=high).             |
| `icc_color_profile` | `bool`  | `true`  | Enable ICC color profile support.                     |

---

## Navigation & Behavior

Control default modes and UI behavior:

| Setting                  | Type     | Default                                | Description                                                                     |
| ------------------------ | -------- | -------------------------------------- | ------------------------------------------------------------------------------- |
| `initial_selection_mode` | `string` | `"region"`                             | Selection mode at startup: "region", "text", "annot_highlight", "annot_select". |
| `remember_last_visited`  | `bool`   | `true`                                 | Go to last visited page when reopening a file.                                  |
| `page_history`           | `int`    | `100`                                  | Number of visited pages to remember per session.                                |
| `confirm_on_quit`        | `bool`   | `true`                                 | Prompt before quitting dodo.                                                    |
| `auto_resize`            | `bool`   | `false`                                | Auto-resize pages to fit the window.                                            |
| `zoom_factor`            | `float`  | `1.25`                                 | Factor used for zoom in/out.                                                    |
| `initial_fit`            | `string` | `"width"`                              | Initial page fit mode: "none", "width", "height", "window".                     |
| `invert_mode`            | `bool`   | `true`                                 | Start in inverted color mode.                                                   |
| `synctex_editor_command` | `string` | "kitty -e nvim --remote-silent +%l %f" | Command to open synced TeX editor.                                              |
| `auto_reload`            | `bool`   | `true`                                 | Reload the document if file changes externally.                                 |
| `recent_files`           | `bool`   | `true`                                 | Keep a list of recently opened files.                                           |
| `num_recent_files`       | `int`    | `25`                                   | Maximum number of recent files (-1 for unlimited).                              |

---

## Keyboard Shortcuts

Customize keybindings in `[keybindings]`. Examples:

| Action              | Default Shortcut | Description                   |
| ------------------- | ---------------- | ----------------------------- |
| Open file           | `o`              | Open a PDF file.              |
| Close file          | `Ctrl+w`         | Close the current file.       |
| Link hint visit     | `f`              | Jump to a link.               |
| Link hint copy      | `Shift+F`        | Copy link URL.                |
| Scroll left         | `h`              | Horizontal scroll left.       |
| Scroll down         | `j`              | Scroll down.                  |
| Scroll up           | `k`              | Scroll up.                    |
| Scroll right        | `l`              | Horizontal scroll right.      |
| Previous location   | `Ctrl+o`         | Jump back in history.         |
| Next page           | `Shift+J`        | Go to next page.              |
| Previous page       | `Shift+K`        | Go to previous page.          |
| First page          | `g,g`            | Jump to first page.           |
| Last page           | `Shift+G`        | Jump to last page.            |
| Search              | `/`              | Open search input.            |
| Search this page    | `?`              | Limit search to current page. |
| Zoom in/out         | `=` / `-`        | Zoom the page.                |
| Zoom reset          | `0`              | Reset zoom to default.        |
| Toggle invert color | `i`              | Switch to inverted mode.      |
| Fullscreen          | `F11`            | Toggle fullscreen.            |
| Cancel selection    | `escape`         | Cancel current selection.     |
| Save file           | `Ctrl+S`         | Save annotations or changes.  |
| Select all          | `Ctrl+A`         | Select all text/annotations.  |

> Note: You can redefine any key by modifying the `[keybindings]` section in `config.toml`.

---

## Notes

* Colors are in **ARGB format**; transparency can be controlled with the alpha channel (`AA`).
* DPI and DPR should match your display for crisp rendering.
* Selection and annotation modes are controlled via `initial_selection_mode` and can be toggled in the UI.
* Always restart dodo after modifying `config.toml` to apply changes.

