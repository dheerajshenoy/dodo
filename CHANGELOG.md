# dodo

## 0.4.0

- Implement document reloading on external changes
- Remove command bar

## 0.3.1

### Features

- Undo/Redo support for annotations
    - Rect annotations
    - Text highlight annotations
    - Delete annotations
- Multi-click text selection (double, triple, quadruple click for word, line, paragraph)
- Page rotation (clockwise and counter-clockwise)
- Editable page number in panel (click to edit and navigate)
- Tab navigation keybindings (tab_next, tab_prev, tab1-tab9)
- Config options:
    - Behavior
        - `undo_limit` - Maximum number of undo operations to keep

### Bug Fixes

- Jump marker fading animation fix
- Fix unsaved changes being asked multiple times
- Centralized unsaved handling

## 0.3.0

### Features

- Fix copy text popup when no text is selected
- Wonky selection handling
- Fix scaling problem in single monitor systems
- Multi-monitor user defined scaling support
- Region select mode
    - Copy text from selected region to clipboard
    - Copy selected region as image to clipboard
    - Save selected region as jpeg/png etc. image
    - Open selected region externally
- Image right click context menu
    - Copy image to clipboard
    - Save image as jpeg/png/psd
    - Open image externally
- Display Pixel Ratio user config option (fix blurry text on high DPI screens)
- Outline widget as a sidebar option
- Outline widget page numbers displayed properly
- Outline hierarchy search support
- Config options:
    - UI
        - `outline_as_side_panel`
        - `outline_panel_width`
        - `link_hint_size`
    - Rendering
        - `dpr`
- Popup annotations improved

### Bug Fixes

- Fix buggy selection behavior after clicking
- Fix blurry text on high DPI screens
- Fix popup annotation not deleting properly

## 0.2.4

- Fix drag and drop not opening files
- Popup Annotation support
- Page navigation menu disable unintended buttons

## 0.2.3

- Fix startup tab on launch with files
- Vim-like search in commandbar
- Goto page from commandbar
- Fix goto page
- Fix link clicking mouse release event
- Fix recent files addition
- Fix panel and other actions updating on tab switching

## 0.2.2

- Tabs support
- Sessions support

## 0.2.1

- Bug fixes

## 0.2.0

- Page visit history
- Link visit
    - http/https links
    - Page links
    - XYZ links
- Browse links implemented (URL)
- Basic rectangle annotations
- Search
- Table of Contents
- File properties
- Keyboard link navigation


## 0.1.0

- Render
- Navigation
- Panel
