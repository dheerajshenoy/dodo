#!/bin/sh
set -e
SKIP_MUPDF_BUILD=0
PREFIX="/usr/local"
# ============================================================
# Arguments
# ============================================================

for arg in "$@"; do
    case "$arg" in
        --skip-mupdf-build)
            SKIP_MUPDF_BUILD=1
            ;;
        *)
            PREFIX="$arg"
            ;;
    esac
done

# ============================================================
# Config
# ============================================================
STAGE_DIR="$(pwd)/_stage"

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
        echo "Downloading MuPDF $MUPDF_VERSION..."
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
        prefix=$PREFIX

    make install DESTDIR="$STAGE_DIR"

    cd - >/dev/null
    echo "MuPDF built."
}

build_dodo() {
    echo "Building dodo..."

    rm -rf build
    mkdir build
    cd build

    cmake .. \
        -G Ninja \
        -DCMAKE_INSTALL_PREFIX=$PREFIX \
        -DCMAKE_BUILD_TYPE=Release

    ninja
    DESTDIR="$STAGE_DIR" ninja install

    cd - >/dev/null
    echo "dodo built."
}

install_desktop_file() {
    echo "Preparing desktop file..."

    sed "s|@PREFIX|$PREFIX/bin|g" dodo-template.desktop > "$STAGE_DIR/usr/local/share/applications/dodo.desktop"
}

final_install() {
    echo "Installing files to $PREFIX (requires sudo)..."

    sudo mkdir -p "$PREFIX"
    sudo cp -a "$STAGE_DIR"/usr/local/* "$PREFIX"/

    echo "Installation complete."
}

# ============================================================
# Workflow
# ============================================================

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/$PREFIX/share/applications"

if [ "$SKIP_MUPDF_BUILD" -eq 0 ]; then
    download_mupdf
    extract_mupdf
    build_mupdf
else
    echo "Skipping MuPDF build."
fi
build_dodo
install_desktop_file
final_install
