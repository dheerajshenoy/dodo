<p align="center">
    <img src="./resources/dodo-rounded.svg" height="200px" width="200px"/><br><br>
<b><i>A fast and unintrusive PDF reader.</i></b>
</p>

## Why ?

- Because okular is bloated and it is the only good pdf reader that seems to exist.
- Also, because I always wanted to create a PDF reader because when I am not developing a
PDF reader, I am reading lots of PDFs for my work.

> [!WARNING]
> Dodo is currently in alpha and may experience crashes or instability.

## Features

- Fast rendering with MuPDF backend

https://github.com/user-attachments/assets/a4c9ae7e-401e-4b02-9ae1-b70ff10079ab

- **SyncTeX** support
- Caching and pre-fetching pages for faster page rendering
- Vim-like keybindings
- Configured using TOML language
![config](https://github.com/user-attachments/assets/59195e30-30dd-487f-8ef5-43c883063d91)
- Faster search
- Visited file location saving
- Link awareness
- Save annotations (highlights only for now)
- Text highlight
- Jump Marker (to help locate where link takes you to)

https://github.com/user-attachments/assets/0f2c99ec-8b82-4c9b-9f41-e0af971c41aa

## Dependencies

- Qt6
- mupdf
- libsynctex \[(optional)for reverse searching and goto reference from LaTeX\]
- cmake (for building)
- ninja (for building)

## Installation

1. Install the required dependencies

    a. For Arch Linux Users

    ``sudo pacman -Syu qt6-base mupdf mupdf-tools libsynctex cmake ninja``

    b. For Debian/Ubuntu Users

        You can install all the dependencies except for mupdf which has to be downloaded and built from source

        ``sudo apt install qt6-base-dev libsynctex-dev cmake ninja-build``

        1. Get the mupdf source code from the [release](https://mupdf.com/releases) page.
        2. Extract and run the following command inside the directory

        ```bash
        make HAVE_X11=no prefix=/usr build=release -j$(nproc)
        sudo make install
        ```

2. Clone this repo
3. Once inside the repo directory, run the following commands

```bash
mkdir build &&
cd build &&
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr &&
ninja &&
sudo ninja install
```

## Configuration

Dodo will search for `config.toml` file in the `~/.config/dodo` directory, and load it if it exists.
Check out the `config.toml` file for reference

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

## Tools Used

- **Neovim** - my beloved text editor, without which I wouldn't be so fast in my commits :)
- **ChatGPT** - ofc, it's 2025, I'm not gonna lie...
- Qt forums and other websites
- MuPDF discord channel
