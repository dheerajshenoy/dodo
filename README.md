<p align="center">
    <img src="./resources/dodo-rounded.png" height="200px" width="200px"/><br><br>
<b><i>A fast and unintrusive PDF reader.</i></b>
</p>

[![GitHub License](https://shields.io/badge/LICENSE-AGPL-3)](https://opensource.org/license/agpl-v3)

> [!WARNING]
> Dodo is currently in alpha and may experience crashes or instability.

**Dodo** is a lightweight, high-performance PDF reader designed to provide a smooth and
efficient document viewing experience. Built using MuPDF and Qt, it supports fast rendering,
precise text selection, annotation handling (highlights and rects), and advanced features like
SyncTeX integration for LaTeX users. Whether you're reading academic papers, technical manuals, or
everyday documents, this reader offers a streamlined interface and robust functionality with minimal
resource usage.

## Table of Contents

- [Features](#features)
- [Dependencies](#dependencies)
- [Installation](#installation)
- [Usage](#usage)
- [Configuration](#configuration)
- [Theming dodo](#theming-dodo)
- [Spotlight](#spotlight)
- [TODO](#todo)
- [Contributing](#contributing)

## Features

- Fast rendering with MuPDF backend
- **SyncTeX** support
- Caching and pre-fetching pages for faster page rendering
- Vim-like keybindings
- Configured using TOML language
- Faster search
- Visited file location saving
- Link awareness
- Save annotations (highlights only for now)
- Text highlight
- Jump Marker (to help locate where link takes you to)

- Wayland support

## Dependencies

- Qt6
- mupdf
- libsynctex \[for reverse searching and goto reference from LaTeX\]
- cmake (for building)
- ninja (for building)

## Installation

1. Install the required dependencies

    a. **Arch Linux Users**

    ``sudo pacman -Syu qt6-base mupdf mupdf-tools libsynctex cmake ninja``

    b. **Debian/Ubuntu Users**

    You can install all the dependencies except for mupdf which has to be downloaded and built from source

    ``sudo apt install qt6-base-dev libsynctex-dev cmake ninja-build``

    MuPDF:

    1. Get the mupdf source code from the [release](https://mupdf.com/releases) page.
    2. Extract and run the following command inside the directory

    ```bash
    sudo make HAVE_X11=no HAVE_GLUT=no prefix=/usr build=release -j$(nproc) install
    ```

3. Clone this repo
4. Once inside the repo directory, run the following commands

```bash
mkdir build
cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
ninja
sudo ninja install
```

# Usage

Check out [usage](./USAGE.md) to learn how to use dodo.

## Configuration

Dodo will search for `config.toml` file in the `~/.config/dodo` directory, and load it if it exists.
Check out the `config.toml` file for reference

## Theming dodo

**Kvantum (Recommended)**

Kvantum is a powerful SVG-based theme engine for Qt.
It supports dark themes, custom widgets, and integrates well with Qt6.

Check out kvantum's install page [here](https://github.com/tsujan/Kvantum/blob/master/Kvantum/INSTALL.md)

Once installed, you can select a theme you like in the `kvantummanager` app and
add `QT_STYLE_OVERRIDE=kvantum` env variable. On the next launch, you should have
the specified theme loaded in dodo.

## Spotlight

Demo video of few of the features of dodo

### Demo of speed

https://github.com/user-attachments/assets/a4c9ae7e-401e-4b02-9ae1-b70ff10079ab

### Jump Marker

https://github.com/user-attachments/assets/0f2c99ec-8b82-4c9b-9f41-e0af971c41aa

## TODO

- [ ] OCR support
- [ ] Tabs support
- [X] Text mouse selection
- [X] Synctex support
- [X] HiDPI support
- [X] Annotations
    - [X] Basic rectangle highlight
    <!-- - [ ] Word aware highlight -->
- [X] Table of Contents
- [X] Keyboard link navigation
- [ ] Complex searches
- [X] Visited file location
- [X] Keyboard navigation
- [X] Search (with highlight)
- [X] Clicking figure names
- [ ] Undo/Redo operations
- [ ] Accelerator

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you'd like to change.



