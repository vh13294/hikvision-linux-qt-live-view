#!/usr/bin/env bash
# Run this script from the code-server terminal to build both demos.
# Usage:  bash /workspace/sdk/build-demos.sh [qt|console|all]
set -e

SDK=/workspace/sdk
TARGET=${1:-all}

# ── Qt5 fix: QtClientDemo.pro uses Qt4 module layout. ─────────────────────────
# Qt5 moved QWidget/QDialog/etc. into the 'widgets' module.
# We patch the .pro to add 'widgets' only if it is missing.
PRO=$SDK/QtDemo/Linux64/QtCreator/QtClientDemo.pro
if ! grep -q 'widgets' "$PRO"; then
    echo "[patch] Adding 'widgets' to QT += in $PRO"
    sed -i 's/QT +=\(.*\)opengl/QT +=\1opengl \\\n\twidgets/' "$PRO"
fi

# Build outputs go under ~/build/ so the developer user can always write there,
# regardless of the ownership of the mounted /workspace/sdk volume.
BUILD_BASE=${BUILD_BASE:-$HOME/build}

# ── Console demo ───────────────────────────────────────────────────────────────
build_console() {
    echo "=== Building consoleDemo ==="
    local out="$BUILD_BASE/console"
    mkdir -p "$out/obj" "$out/lib"
    cd "$SDK/consoleDemo/linux64/proj"
    make -j"$(nproc)" OBJDIR="$out/obj" LIBDIR="$out/lib"
    echo "→ Output: $out/lib/sdkTest"
}

# ── Qt demo ────────────────────────────────────────────────────────────────────
build_qt() {
    echo "=== Building QtClientDemo ==="
    local out="$BUILD_BASE/qt"
    mkdir -p "$out"
    # Shadow build: qmake from a writable directory, point at the .pro file
    cd "$out"
    qmake -spec linux-g++ "$SDK/QtDemo/Linux64/QtCreator/QtClientDemo.pro" \
        QMAKE_LIBDIR+="$SDK/lib" \
        QMAKE_RPATHDIR+="$SDK/lib:$SDK/lib/HCNetSDKCom"
    make -j"$(nproc)"
    echo "→ Output: $out/QtClientDemo"
}

case "$TARGET" in
    console) build_console ;;
    qt)      build_qt ;;
    all)     build_console; build_qt ;;
    *)       echo "Usage: $0 [qt|console|all]"; exit 1 ;;
esac

echo "Done."
