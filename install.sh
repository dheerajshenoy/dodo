#!/bin/sh
set -e

# ============================================================
# Config
# ============================================================
PREFIX="${1:-/usr/local}"
MUPDF_VERSION="1.27.0"
MUPDF_URL="https://mupdf.com/downloads/archive/mupdf-${MUPDF_VERSION}-source.tar.gz"
EXTERN_DIR="extern"
MUPDF_TARBALL="${EXTERN_DIR}/mupdf-${MUPDF_VERSION}-source.tar.gz"
MUPDF_SRC_DIR="${EXTERN_DIR}/mupdf"

echo "Installing dodo with prefix: $PREFIX"

# ============================================================
# Functions
# ============================================================

download_mupdf() {
    mkdir -p "$EXTERN_DIR"

    if [ ! -f "$MUPDF_TARBALL" ]; then
        echo "Downloading MuPDF $MUPDF_VERSION ..."
        wget -O "$MUPDF_TARBALL" "$MUPDF_URL"
    else
        echo "MuPDF source already downloaded."
    fi
}

extract_mupdf() {
    echo "Extracting MuPDF..."

    rm -rf "$MUPDF_SRC_DIR"
    tar -xf "$MUPDF_TARBALL" -C "$EXTERN_DIR"
    mv "${EXTERN_DIR}/mupdf-${MUPDF_VERSION}-source" "$MUPDF_SRC_DIR"
}

build_mupdf() {
    echo "Building MuPDF..."
    cd "$MUPDF_SRC_DIR"

    make -j"$(nproc)" \
    HAVE_X11=no \
    HAVE_GLUT=no \
    build=release \
    prefix="$PREFIX"

    echo "Installing MuPDF..."
    if [ -w "$PREFIX" ]; then
        make install prefix="$PREFIX"
    else
        sudo make install prefix="$PREFIX"
    fi

    cd - >/dev/null
    echo "MuPDF built successfully."
}

build_dodo() {
    echo "Building dodo..."

    rm -rf build
    mkdir build
    cd build

    cmake .. \
    -G Ninja \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_BUILD_TYPE=Release

    ninja

    if [ -w "$PREFIX" ] || [ -w "$(dirname "$PREFIX")" ]; then
        ninja install
    else
        echo "Installing to $PREFIX requires sudo."
        sudo ninja install
    fi

    cd - >/dev/null
    echo "dodo installed successfully."
}

# ============================================================
# Workflow
# ============================================================
download_mupdf
extract_mupdf
build_mupdf
build_dodo
