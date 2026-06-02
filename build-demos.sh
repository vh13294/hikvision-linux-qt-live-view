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

# ── Console demo ───────────────────────────────────────────────────────────────
build_console() {
    echo "=== Building consoleDemo ==="
    cd "$SDK/consoleDemo/linux64/proj"
    mkdir -p obj ../lib
    make -j"$(nproc)"
    echo "→ Output: $SDK/consoleDemo/linux64/lib/sdkTest"
}

# ── Qt demo ────────────────────────────────────────────────────────────────────
build_qt() {
    echo "=== Building QtClientDemo ==="
    cd "$SDK/QtDemo/Linux64/QtCreator"
    mkdir -p obj/Gui obj/Moc obj/Obj ../lib
    # -spec linux-g++ ensures Qt5 spec is used
    qmake -spec linux-g++ QtClientDemo.pro \
        QMAKE_LIBDIR+="$SDK/lib" \
        QMAKE_RPATHDIR+="$SDK/lib:$SDK/lib/HCNetSDKCom"
    make -j"$(nproc)"
    echo "→ Output: $SDK/QtDemo/Linux64/lib/QtClientDemo"
}

case "$TARGET" in
    console) build_console ;;
    qt)      build_qt ;;
    all)     build_console; build_qt ;;
    *)       echo "Usage: $0 [qt|console|all]"; exit 1 ;;
esac

echo "Done."
