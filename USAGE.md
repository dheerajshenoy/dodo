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
- [Annotations](#annotations)
- [SyncTeX Integration](#synctex-integration)
- [Configuration](#configuration)
- [Keybindings](#keybindings)
- [Theming](#theming)
- [Troubleshooting](#troubleshooting)
- [FAQ](#faq)
- [Getting Help](#getting-help)

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
| b              | invert_color      |
| f              | link_hint_visit   |
| Ctrl+s         | save              |
| Alt+1          | text_highlight    |
| Alt+2          | annot_rect        |
| Alt+3          | annot_edit        |
| t              | outline           |
| /              | search            |
| n              | search_next       |
| Shift+n        | search_prev       |
| +              | zoom_in           |
| -              | zoom_out          |
| 0              | zoom_reset        |
| Ctrl+o         | prev_location     |
| o              | open              |
| h              | scroll_left       |
| j              | scroll_down       |
| k              | scroll_up         |
| l              | scroll_right      |
| Shift+j        | next_page         |
| Shift+k        | prev_page         |
| g,g            | first_page        |
| Shift+g        | last_page         |

## Commands

about
annot_edit
annot_rect
auto_resize
cancel_selection
close_file
command
file_properties
fit_height
fit_width
fit_window
first_page
fullscreen
goto_page
invert_color
keybindings
last_page
link_hint_copy
link_hint_visit
next_page
outline
open_file
prev_location
prev_page
reload
redo
rotate_anticlock
rotate_clock
save
save_as
scroll_down
scroll_left
scroll_right
scroll_up
search
search_next
search_prev
setdpr
tabclose
tabgoto
tabnext
tabprev
text_highlight
text_highlight_current_selection
toggle_menubar
toggle_panel
toggle_tabs
undo
yank
yank_all
zoom_in
zoom_out
zoom_reset

## Search

* Press `/` to enter search mode.

* Type your search term and press Enter.

* Matches will be highlighted.

* Use `n` and `N` to jump forward and backward between matches.

* To cancel the current search and clear all the search matches, **search for an empty string**.

# Annotations

* Select the annotations tool from `Tools`

# SyncTeX Integration

For LaTeX users, Dodo supports SyncTeX for source-to-PDF and PDF-to-source syncing.

From your LaTeX editor, configure SyncTeX to open Dodo for reverse search.
The command to launch dodo with synctex document is by passing the ``--synctex-forward`` argument.

It has the following format:
``Format: --synctex-forward={pdf-file-path}#{src-file-path}:{line}:{column}``
