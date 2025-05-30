<p align="center">
    <img src="./resources/dodo-rounded.png" height="200px" width="200px"/><br><br>
<b><i>A fast and unintrusive PDF reader.</i></b>
</p>

[![GitHub License](https://shields.io/badge/LICENSE-AGPL-3)](https://opensource.org/license/agpl-v3)

**Dodo** is a lightweight, high-performance PDF reader designed to provide a smooth and
efficient document viewing experience. Built using MuPDF and Qt, it supports fast rendering,
precise text selection, annotation handling (highlights and rects), and advanced features like
SyncTeX integration for LaTeX users. Whether you're reading academic papers, technical manuals, or
everyday documents, this reader offers a streamlined interface and robust functionality with minimal
resource usage.

> [!WARNING]
> Dodo is currently in alpha and may experience crashes or instability.

> [!NOTE]
> dodo version is v0.2.2-alpha

## Table of Contents

- [Features](#features)
- [Dependencies](#dependencies)
- [Installation](#installation)
    - [Arch Linux](#arch-linux)
    - [Debian/Ubuntu](#debianubuntu)
    - [Others](#others)
- [Usage](#usage)
- [Configuration](#configuration)
- [Theming dodo](#theming-dodo)
- [Spotlight](#spotlight)
- [TODO](#todo)
- [Contributing](#contributing)
- [FAQ](#faq)

## Features

- Fast rendering with MuPDF backend
- **SyncTeX** support
- Caching and pre-fetching pages for faster page rendering
- Vim-like keybindings
- Configured using TOML language
- Tabs support
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

> [!NOTE]
> There are two branches in dodo.
>
> 1. `main (default)`
> 2. `untabbed`
>
> `untabbed` is dodo without tab support and many other features present in the main branch like session management etc.
> No further updates will be pushed to the untabbed branch. If you don't want tabs, go for `untabbed` branch else stick with
> the default branch.

> [!NOTE]
> The `install.sh` script installs latest version of MuPDF library required for dodo

### Arch Linux

```bash
sudo pacman -Syu qt6-base libsynctex cmake ninja
git clone https://github.com/dheerajshenoy/dodo
cd dodo
./install.sh
```

### Debian/Ubuntu

```bash
sudo apt update && sudo apt install qt6-base-dev libsynctex-dev cmake ninja-build
git clone https://github.com/dheerajshenoy/dodo
cd dodo
./install.sh
```

### Others

1. Install the required dependencies
2. Clone the repository and build similar to the Arch Linux build approach.

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

Check out [TODO.md](./TODO.md)

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you'd like to change.

## FAQ

#### Does Dodo support tabs?
Not yet. Tabs support is planned.

#### Can I use Dodo on Wayland?
Yes, Dodo supports Wayland natively.

#### How do I report bugs?
Open issues on the GitHub repository here.


