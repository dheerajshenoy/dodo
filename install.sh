#!/bin/sh

set -e

PREFIX="${1:-/usr/}"

echo "Installing dodo with install prefix: $PREFIX"

# === Configuration ===
MUPDF_VERSION="1.26.1"
MUPDF_URL="https://mupdf.com/downloads/archive/mupdf-${MUPDF_VERSION}-source.tar.gz"
MUPDF_DIR="extern/mupdf"

download_mupdf () {
    mkdir -p extern
    cd extern

    # Check if wget is installed
    if ! command -v wget >/dev/null 2>&1; then
        echo "Error: wget is not installed. Please install it and rerun the script."
        exit 1
    fi

    echo "Downloading MuPDF ${MUPDF_VERSION}..."
    wget -c "$MUPDF_URL"
    cd ..
}

build_mupdf () {
    cd extern

    if [ -d "$MUPDF_DIR" ]; then
        echo "Extracting MuPDF..."
        tar -xf "mupdf-${MUPDF_VERSION}-source.tar.gz"
        mv "mupdf-${MUPDF_VERSION}-source" mupdf
    else
        echo "Mupdf already extracted"
    fi

    cd mupdf

    # === Step 5: Build ===
    echo "Building MuPDF..."
    make -j$(nproc) HAVE_X11=no HAVE_GLUT=no prefix="$PREFIX" build=release

    echo "MuPDF built successfully."
    cd ../..
}

build_dodo() {
    echo "Building Dodo"

    mkdir -p build
    cd build

    cmake .. -G Ninja -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Release
    ninja

    if [ -w "$PREFIX" ] || [ -w "$(dirname "$PREFIX")" ]; then
        ninja install
    else
        echo "Installing to $PREFIX requires elevated permissions."
        sudo ninja install
    fi
}

download_mupdf
build_mupdf
build_dodo
