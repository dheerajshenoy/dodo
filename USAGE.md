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

| Key      | Action             |
| -------- | ------------------ |
| `j`      | Scroll down        |
| `k`      | Scroll up          |
| `h`      | Scroll left        |
| `l`      | Scroll right       |
| `gg`     | Go to first page   |
| `G`      | Go to last page    |
| `+`      | Zoom In            |
| `-`      | Zoom Out           |
| '/'      | Search             |
| 'n'      | jump to forward match |
| 'Shift+n' | jump to backward match |

## Search

* Press `/` to enter search mode.

* Type your search term and press Enter.

* Matches will be highlighted.

* Use `n` and `N` to jump forward and backward between matches.

# Annotations

* Select the annotations tool from `Tools`

# SyncTeX Integration

For LaTeX users, Dodo supports SyncTeX for source-to-PDF and PDF-to-source syncing.

From your LaTeX editor, configure SyncTeX to open Dodo for reverse search.
The command to launch dodo with synctex document is by passing the ``--synctex-forward`` argument.

It has the following format:
``Format: --synctex-forward={pdf-file-path}#{src-file-path}:{line}:{column}``
