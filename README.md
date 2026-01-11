<p align="center">
    <img src="./resources/dodo2.png" height="200px" width="200px"/><br><br>
<b><i>Just another PDF reader, but it's not</i></b>
</p>

[![GitHub License](https://shields.io/badge/LICENSE-AGPL-3)](https://opensource.org/license/agpl-v3)

**Dodo** (pronounced "doh-doh") is a fast, keyboard-friendly PDF reader built for
smooth reading, quick navigation, and deep customization. It uses MuPDF for
rendering and Qt for the UI, so it stays lightweight while still offering tabs,
search, annotations, and power-user workflows.

If you read papers, manuals, or long PDFs and want speed + control, this is for you.

> [!NOTE]
> Latest dodo version is v0.5.3

## Table of Contents

- [Screenshots](#screenshots)
- [Features](#features)
- [Dependencies](#dependencies)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Configuration](#configuration)
- [Theming dodo](#theming-dodo)
- [TODO](#todo)
- [Contributing](#contributing)
- [FAQ](#faq)

## Screenshots

### Home screen
<img src="./images/home.png" alt="dodo home screen" width="800"/>

### Scrolling View
<img src="./images/Scrolling.gif" alt="Scrolling" width="800"/>

### Layouts
<img src="./images/Layouts.gif" alt="Layouts" width="800"/>

### File opened + outline panel
<img src="./images/dodo.png" alt="File + Outline" width="800"/>

### Search Marker in Scrollbar
<img src="./images/Search-Hits-Scrollbar.gif" alt="Search Hits in Scrollbar" width="800"/>

### Jump Marker feature in action
<img src="./images/Jump-Marker.gif" alt="dodo jump marker feature" width="800"/>

### Synctex forward search from dodo to tex editor (Zed editor in this case)
<img src="./images/synctex.gif" alt="dodo synctex forward search feature" width="800"/>
</p>

## Features

- Fast rendering with MuPDF backend
- Tabs, recent files, and session restore
- Search with highlight markers on the scrollbar
- Outline (table of contents) panel
- Annotation tools: highlight, rectangle, popup
- Region selection: copy text, copy as image, save, or open externally
- Link detection and keyboard link hints
- SyncTeX support for LaTeX workflows
- Fully configurable keybindings
- Smooth scrolling and page caching
- Optional LLM widget (compile-time flag)

## Dependencies

- Qt6
- MuPDF
- synctex (for reverse search and goto reference from LaTeX)
- CMake (for building)
- Ninja (for building)

### Dependency for building MuPDF

- glu

## Installation

> [!NOTE]
> The `install.sh` script installs the MuPDF library version required by dodo.

> [!WARNING]
> If the newest MuPDF release breaks the build, open `install.sh` and switch to a
> known working version.

> [!NOTE]
> By default dodo is installed to `/usr/local/`. You can change this by passing a
> different prefix to `install.sh`, for example:
>
>```bash
>./install.sh /usr
>```

### Arch Linux

```bash
sudo pacman -Syu qt6-base qt6-svg libsynctex cmake ninja glu
git clone https://github.com/dheerajshenoy/dodo
cd dodo
./install.sh
```

### Debian/Ubuntu

```bash
sudo apt update && sudo apt install qt6-base-dev qt6-svg-dev libsynctex-dev cmake ninja-build freeglut3-dev
git clone https://github.com/dheerajshenoy/dodo
cd dodo
./install.sh
```

### Others

1. Install the required dependencies
2. Clone the repository and build similar to the Arch Linux build approach.

> [!NOTE]
> If text looks blurry, set `rendering.dpr` in `config.toml` to match your
> display scale factor (for example, `1.25` for 125%).

## Quick Start

1. Launch dodo and open a PDF.
2. Press `Ctrl+Shift+P` to open the command palette.
3. Edit `config.toml` to customize the UI and shortcuts.

## Configuration

See [CONFIGURATION.md](./CONFIGURATION.md) for a clear, beginner-friendly guide to
all settings.

## Theming dodo

**Kvantum (Recommended)**

Kvantum is a powerful SVG-based theme engine for Qt. It supports dark themes,
custom widgets, and integrates well with Qt6.

Check out Kvantum's install page here:
https://github.com/tsujan/Kvantum/blob/master/Kvantum/INSTALL.md

Once installed, select a theme in `kvantummanager` and set:

```bash
QT_STYLE_OVERRIDE=kvantum
```

## TODO

Check out [TODO.md](./TODO.md)

## Contributing

Pull requests are welcome. For major changes, please open an issue first to
discuss what you'd like to change.

## FAQ

#### Can I use Dodo on Wayland?
Yes, Dodo supports Wayland natively.

#### How do I report bugs?
Open issues on the GitHub repository here.
