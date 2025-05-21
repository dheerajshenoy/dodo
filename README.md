# DODO

<img src="./logo/banner.png"/>
A fast and minimal PDF reader.

## Features

- Fast MuPDF backend
- Caching and pre-fetching pages for faster page rendering
- Vim-like keybindings
- configured using TOML language
- Faster search
- Visited file location saving
- Link awareness
- Save annotations

## Dependencies

- Qt6
- mupdf
- cmake (for building)
- make (for building)

## Installation

1. Install the required dependencies
2. Clone this repo
3. Once inside the repo directory, run the following commands

```bash
mkdir build &&
cd build &&
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr &&
make -j$(nproc) &&
sudo make install
```

## 🖮 Default Keybindings

| Shortcut     | Action             |
|--------------|--------------------|
| Ctrl+S       | Save file          |
| Alt+1        | Toggle highlight   |
| T            | Table of contents  |
| Escape       | Escape             |
| /            | Search             |
| N            | Next search hit    |
| Shift+N      | Previous search hit|
| Ctrl+O       | Go back in history |
| O            | Open file          |
| J            | Scroll down        |
| K            | Scroll up          |
| H            | Scroll left        |
| L            | Scroll right       |
| Shift+J      | Next page          |
| Shift+K      | Previous page      |
| G,G          | First page         |
| Shift+G      | Last page          |
| 0            | Reset zoom         |
| =            | Zoom in            |
| -            | Zoom out           |
| <            | Rotate counterclockwise |
| >            | Rotate clockwise   |

## Configuration

Dodo will search for `config.toml` file in the `~/.config/dodo` directory, and load it if it exists.
Check out the `config.toml` file for reference

## TODO

- [ ] Annotations
    - [X] Basic rectangle highlight
    - [ ] Word aware highlight
- [X] Table of Contents
- [ ] Keyboard link navigation
- [ ] Complex searches
- [X] Visited file location
- [X] Keyboard navigation
- [X] Search (with highlight)
