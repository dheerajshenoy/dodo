#!/bin/sh
set -eu

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
EXTERN_DIR="$ROOT_DIR/external"
BUILD_DIR="$ROOT_DIR/build"
PREFIX="/app"
BUILD_TYPE="Release"
ENABLE_LLM_SUPPORT="OFF"

MUPDF_VERSION="1.27.0"
MUPDF_TARBALL="$EXTERN_DIR/mupdf-${MUPDF_VERSION}-source.tar.gz"
MUPDF_SRC_DIR="$EXTERN_DIR/mupdf-${MUPDF_VERSION}-source"
MUPDF_BUILD_DIR="$EXTERN_DIR/mupdf"
MUPDF_BUILD="release"

die() { echo "Error: $*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

if [ ! -f "$MUPDF_TARBALL" ]; then
    die "MuPDF tarball missing: $MUPDF_TARBALL (add it as a Flatpak source)"
fi

if have getconf; then
    JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
else
    JOBS=1
fi

have cmake || die "cmake not found"
have make || die "make not found"

if have ninja; then
    GEN="-G Ninja"
    BUILD_CMD="ninja"
    INSTALL_CMD="ninja install"
else
    GEN=""
    BUILD_CMD="cmake --build . -- -j$JOBS"
    INSTALL_CMD="cmake --install ."
fi

extract_mupdf() {
    echo "Extracting MuPDF..."
    rm -rf "$MUPDF_SRC_DIR" "$MUPDF_BUILD_DIR"
    tar -xf "$MUPDF_TARBALL" -C "$EXTERN_DIR" || die "tar failed"
    [ -d "$MUPDF_SRC_DIR" ] || die "Expected extracted dir not found: $MUPDF_SRC_DIR"
    mv "$MUPDF_SRC_DIR" "$MUPDF_BUILD_DIR"
}

build_mupdf() {
    echo "Building MuPDF ($MUPDF_BUILD)..."
    [ -d "$MUPDF_BUILD_DIR" ] || die "MuPDF dir missing: $MUPDF_BUILD_DIR"

  # LCMS_FLAG="HAVE_LCMS=no"   # disables ICC color management if needed
  LCMS_FLAG=""

  (cd "$MUPDF_BUILD_DIR" && \
      make -j"$JOBS" build="$MUPDF_BUILD" HAVE_X11=no HAVE_GLUT=no $LCMS_FLAG)

  [ -f "$MUPDF_BUILD_DIR/build/$MUPDF_BUILD/libmupdf.a" ] || die "MuPDF lib missing"
  [ -f "$MUPDF_BUILD_DIR/build/$MUPDF_BUILD/libmupdf-third.a" ] || die "MuPDF third lib missing"
}

mupdf_built() {
    [ -f "$MUPDF_BUILD_DIR/build/$MUPDF_BUILD/libmupdf.a" ] && \
    [ -f "$MUPDF_BUILD_DIR/build/$MUPDF_BUILD/libmupdf-third.a" ]
}

build_dodo() {
    echo "Building dodo..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

  # shellcheck disable=SC2086
  cmake .. $GEN \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DENABLE_LLM_SUPPORT="$ENABLE_LLM_SUPPORT"

  sh -c "$BUILD_CMD"
  sh -c "$INSTALL_CMD"
  cd "$ROOT_DIR"
}

echo "[1/2] MuPDF"
if mupdf_built; then
    echo "MuPDF already built, skipping"
else
    extract_mupdf
    build_mupdf
fi

echo "[2/2] dodo"
build_dodo

rm -rf "$BUILD_DIR" "$MUPDF_BUILD_DIR/build"
