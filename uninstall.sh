#!/bin/sh

# uninstall.sh - Uninstaller script for dodo

set -e

printf "Starting uninstallation...\n"

read -p "Enter the install prefix used during the install (default: /usr): " PREFIX
PREFIX=${PREFIX:-/usr}

read -p "Are you sure you want to uninstall 'dodo' and all of it's assciated files ? [y/N]" confirm
confirm="${confirm:-n}"
confirm="${confirm,,}"

if [[ "$confirm" == "n" ]]; then
    echo "Cancelling uninstall"
    exit 0
fi

if ! command -v iv > /dev/null; then
    echo "dodo is not installed, exiting uninstallation"
    exit 0
fi


# List of installed files relative to prefix - customize these
FILES=(
    "bin/dodo"
    "share/applications/dodo.desktop"
    "share/icons/hicolor/16x16/apps/dodo.png"   # Example icon path
    "share/icons/hicolor/32x32/apps/dodo.png"   # Example icon path
    "share/icons/hicolor/48x48/apps/dodo.png"   # Example icon path
    "share/icons/hicolor/64x64/apps/dodo.png"   # Example icon path
    "share/icons/hicolor/128x128/apps/dodo.png"   # Example icon path
    "share/icons/hicolor/256x256/apps/dodo.png" # Add other icon sizes as needed
    "share/icons/hicolor/512x512/apps/dodo.png" # Add other icon sizes as needed
)

for file in "${FILES[@]}"; do
    TARGET="$PREFIX/$file"
    if [ -e "$TARGET" ]; then
        echo "Removing $TARGET"
        rm -rf "$TARGET"
    else
        echo "File $TARGET not found, skipping."
    fi
done

