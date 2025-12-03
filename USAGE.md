# Usage Guide for Dodo PDF Reader

Welcome to **Dodo**, a fast and lightweight PDF reader built with MuPDF and Qt6.
This guide covers everything you need to know to get started, use core features,
configure, and troubleshoot.

> [!NOTE]
> This document assumes that you have not configured dodo and mentions all the default keybindings.

---

## Table of Contents

- [Launching Dodo](#launching-dodo)
- [Keybindings](#keybindings)
- [Commands](#commands)
- [Search](#search)
- [Text Selection](#text-selection)
- [Annotations](#annotations)
- [Page Rotation](#page-rotation)
- [Tab Navigation](#tab-navigation)
- [SyncTeX Integration](#synctex-integration)

    ---

## Launching Dodo

After installation, run Dodo from your terminal or application launcher.

```bash
dodo [options] [file.pdf]
```

Launch Dodo with no file open.
`dodo`

Open myfile.pdf on startup.
`dodo myfile.pdf`

Show command line help.
`dodo --help`

## Keybindings

| Shortcut       | Action            |
|----------------|-------------------|
| Ctrl+Shift+m   | toggle_menubar    |
| Ctrl+Shift+b   | toggle_panel      |
| Ctrl+Shift+t   | toggle_tabs       |
| i              | invert_color      |
| f              | link_hint_visit   |
| Shift+f        | link_hint_copy    |
| Ctrl+s         | save              |
| 1              | region_select_mode |
| 2              | annot_rect_mode   |
| 3              | annot_edit_mode   |
| t              | outline           |
| /              | search            |
| n              | search_next       |
| Shift+n        | search_prev       |
| =              | zoom_in           |
| -              | zoom_out          |
| 0              | zoom_reset        |
| Ctrl+o         | prev_location     |
| o              | open_file         |
| h              | scroll_left       |
| j              | scroll_down       |
| k              | scroll_up         |
| l              | scroll_right      |
| Shift+j        | next_page         |
| Shift+k        | prev_page         |
| g,g            | first_page        |
| Shift+g        | last_page         |
| b              | goto_page         |
| u              | undo              |
| Ctrl+r         | redo              |
| y              | yank              |
| d              | delete            |
| >              | rotate_clock      |
| <              | rotate_anticlock  |
| Ctrl+.         | tab_next          |
| Ctrl+,         | tab_prev          |
| Alt+1 - Alt+9  | tab1 - tab9       |
| :              | command           |
| Escape         | cancel_selection  |
| F11            | fullscreen        |
| F12            | about             |

## Commands

List of valid commands in dodo:

- `about`
- `annot_edit_mode`
- `annot_rect_mode`
- `auto_resize`
- `cancel_selection`
- `close_file`
- `command`
- `file_properties`
- `fit_height`
- `fit_width`
- `fit_window`
- `first_page`
- `fullscreen`
- `goto_page`
- `invert_color`
- `keybindings`
- `last_page`
- `link_hint_copy`
- `link_hint_visit`
- `next_page`
- `open_containing_folder`
- `open_file`
- `outline`
- `prev_location`
- `prev_page`
- `redo`
- `region_select_mode`
- `reload`
- `rotate_anticlock`
- `rotate_clock`
- `save`
- `save_as`
- `scroll_down`
- `scroll_left`
- `scroll_right`
- `scroll_up`
- `search`
- `search_next`
- `search_prev`
- `setdpr`
- `tab_close`
- `tab_next`
- `tab_prev`
- `tab1` - `tab9`
- `tabgoto`
- `text_highlight_current_selection`
- `text_highlight_mode`
- `toggle_menubar`
- `toggle_panel`
- `toggle_tabs`
- `undo`
- `yank`
- `yank_all`
- `zoom_in`
- `zoom_out`
- `zoom_reset`

## Search

* Press `/` to enter search mode.

* Type your search term and press Enter.

* Matches will be highlighted.

* Use `n` and `N` to jump forward and backward between matches.

* To cancel the current search and clear all the search matches, **search for an empty string**.

## Text Selection

* **Single click**: Place cursor / clear selection
* **Double click**: Select word
* **Triple click**: Select line
* **Quadruple click**: Select paragraph

## Annotations

* Select the annotations tool from `Tools` or use keybindings:
    * `Alt+1` (or `1`): Region select mode
    * `Alt+2` (or `2`): Rectangle annotation mode
    * `Alt+3` (or `3`): Annotation edit/select mode
* Annotations support undo/redo operations (`u` for undo, `Ctrl+R` for redo)

## Page Rotation

* Use `>` to rotate clockwise
* Use `<` to rotate counter-clockwise

## Tab Navigation

* `Ctrl+.` (Ctrl+Period): Next tab
* `Ctrl+,` (Ctrl+Comma): Previous tab
* `Alt+1` through `Alt+9`: Go to specific tab
* Use `:tabgoto <number>` command to go to a specific tab

# SyncTeX Integration

For LaTeX users, Dodo supports SyncTeX for source-to-PDF and PDF-to-source syncing.

From your LaTeX editor, configure SyncTeX to open Dodo for reverse search.
The command to launch dodo with synctex document is by passing the ``--synctex-forward`` argument.

It has the following format:
``Format: --synctex-forward={pdf-file-path}#{src-file-path}:{line}:{column}``
