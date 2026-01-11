Configuration
=============

This document describes the settings available in `config.toml`. The values
shown here reflect the current defaults.

Quick start
-----------

1. Open `config.toml` in a text editor.
2. Change the settings you care about.
3. Save the file and restart dodo (or reload the config if your build supports
   auto-reload).

General notes:
- Boolean values use `true`/`false`.
- Enums are listed in quotes.
- Colors are ARGB hex strings (alpha first), e.g. `#FF0000FF`.

LLM (optional)
--------------

`[llm]` (only used if compiled with LLM support)
- `provider` (string): LLM provider name (e.g. `"ollama"`).
- `model` (string): model name or identifier.
- `max_tokens` (int): maximum tokens in generated responses.

UI
--

`[ui.window]`
- `fullscreen` (bool): start in fullscreen.
- `menubar` (bool): show the menubar.
- `panel` (bool): show the side panel.
- `startup_tab` (bool): show the startup tab.
- `full_file_path_in_panel` (bool): show full file paths in panel.
- `window_title` (string): window title format; `{}` is replaced with base filename.

`[ui.command_palette]`
- `show_shortcuts` (bool): show keyboard shortcuts in the command palette.
- `height` (int): command palette height in pixels.
- `width` (int): command palette width in pixels.

`[ui.llm_widget]`
- `visible` (bool): show LLM widget by default.
- `panel_width` (int): LLM panel width in pixels.

`[ui.layout]`
- `mode` (enum): `"single"`, `"top_to_bottom"`, `"left_to_right"`.
- `initial_fit` (enum): `"none"`, `"width"`, `"height"`, `"window"`.
- `auto_resize` (bool): fit document to window automatically.

`[ui.zoom]`
- `level` (float): default zoom factor.
- `factor` (float): zoom in/out multiplier.

`[ui.selection]`
- `drag_threshold` (int): drag distance in pixels before selection starts.

`[ui.scrollbars]`
- `horizontal` (bool): show horizontal scrollbar.
- `vertical` (bool): show vertical scrollbar.
- `search_hits` (bool): show search hits on scrollbar.

`[ui.markers]`
- `jump_marker` (bool): show jump marker on navigation.

`[ui.links]`
- `boundary` (bool): show link boundary box.
- `detect_urls` (bool): detect URLs in text and make them links.
- `url_regex` (string): regex used when `detect_urls` is true.

`[ui.link_hints]`
- `size` (float): relative scale for link hint labels.

`[ui.tabs]`
- `visible` (bool): show tab bar.
- `auto_hide` (bool): auto-hide when a single tab is open.
- `closable` (bool): allow tabs to be closed.
- `movable` (bool): allow tabs to be reordered.
- `elide_mode` (enum): `"none"`, `"start"`, `"middle"`, `"end"`.
- `bar_position` (enum): `"top"`, `"bottom"`, `"left"`, `"right"`.

`[ui.outline]`
- `visible` (bool): show outline panel.
- `panel_position` (enum): `"left"`, `"right"`.
- `type` (enum): `"overlay"`, `"dialog"`, `"side_panel"`.
- `panel_width` (int): panel width in pixels.

`[ui.highlight_search]`
- `visible` (bool): show search highlights panel.
- `panel_position` (enum): `"left"`, `"right"`.
- `type` (enum): `"overlay"`, `"dialog"`, `"side_panel"`.
- `panel_width` (int): panel width in pixels.

Colors
------

`[colors]` values are ARGB hex strings.
- `accent`: accent color.
- `background`: background color.
- `search_match`: active search match highlight.
- `search_index`: other search results highlight.
- `link_hint_bg`: link hint background.
- `link_hint_fg`: link hint foreground.
- `selection`: selection overlay color.
- `highlight`: generic highlight color.
- `jump_marker`: jump marker color.
- `annot_rect`: annotation rectangle color.
- `annot_popup`: annotation popup color.

Rendering
---------

`[rendering]`
- `dpi` (float): document DPI used for rendering.
- `dpr` (float or map): device pixel ratio. Use a float for a global value or a
  per-display map, e.g. `{ "eDP-1" = 1.25 }`.
- `cache_pages` (int): maximum number of pages to keep cached per document.
- `antialiasing_bits` (int): `4` for good, `8` for high.
- `icc_color_profile` (bool): apply ICC color profile.

Behavior
--------

`[behavior]`
- `initial_mode` (enum): `"region_select_mode"`, `"annot_rect_mode"`,
  `"annot_select_mode"`, `"text_select_mode"`, `"text_highlight_mode"`,
  `"annot_popup_mode"`.
- `always_open_in_new_window` (bool): always open files in a new window.
- `remember_last_visited` (bool): reopen last visited document.
- `page_history` (int): number of page history entries to keep.
- `confirm_on_quit` (bool): confirm before quitting.
- `invert_mode` (bool): start in inverted colors mode.
- `auto_reload` (bool): auto-reload files on change.
- `recent_files` (bool): show recent files.
- `num_recent_files` (int): size of recent files list.
- `undo_limit` (int): max undo entries.
- `synctex_editor_command` (string): SyncTeX editor command.
- `page_nav_with_mouse` (bool, optional): navigate pages with mouse (commented
  out by default).

Keybindings
-----------

`[keybindings]` maps actions to keys. Keys follow Qt-style names like
`Ctrl+S`, `Shift+N`, `Alt+1`, `F11`, or comma-separated sequences like `g,g`.

Action list:
- `command`
- `open_file`
- `close_file`
- `link_hint_visit`
- `link_hint_copy`
- `scroll_left`
- `scroll_down`
- `scroll_up`
- `scroll_right`
- `prev_location`
- `next_page`
- `prev_page`
- `first_page`
- `last_page`
- `goto_page`
- `outline`
- `search`
- `search_this_page`
- `search_next`
- `search_prev`
- `zoom_reset`
- `zoom_in`
- `zoom_out`
- `tab1`
- `tab2`
- `tab3`
- `tab4`
- `tab5`
- `tab6`
- `tab7`
- `tab8`
- `tab9`
- `tab_next`
- `tab_prev`
- `text_select_mode`
- `region_select_mode`
- `annot_rect_mode`
- `annot_edit_mode`
- `text_highlight_mode`
- `annot_pen_mode`
- `annot_popup_mode`
- `delete`
- `file_properties`
- `invert_color`
- `fullscreen`
- `fit_height`
- `fit_width`
- `fit_window`
- `auto_resize`
- `toggle_menubar`
- `toggle_panel`
- `toggle_tabs`
- `cancel_selection`
- `yank`
- `about`
- `save`
- `select_all`
- `rotate_clock`
- `rotate_anticlock`
- `list_text_highlights`
- `undo`
- `redo`
- `toggle_focus_mode`
- `reselect_last_selection`
- `highlight_annot_search`
- `completion_next`
- `completion_prev`
