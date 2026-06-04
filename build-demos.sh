#!/usr/bin/env bash
# Run this script from the code-server terminal to build both demos.
# Usage:  bash /workspace/sdk/build-demos.sh [qt|console|all]
set -e

SDK="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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
BUILD_BASE=${BUILD_BASE:-$SDK/build}

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

    # Shadow build: qmake from a writable directory, point at the .pro file.
    # Use $ORIGIN-relative RPATHs so the binary works wherever it is deployed,
    # without requiring LD_LIBRARY_PATH to be set externally.
    cd "$out"
    qmake -spec linux-g++ "$SDK/QtDemo/Linux64/QtCreator/QtClientDemo.pro" \
        QMAKE_LIBDIR+="$SDK/lib" \
        QMAKE_RPATHDIR+="$SDK/lib:$SDK/lib/HCNetSDKCom" \
        QMAKE_LFLAGS+="-Wl,-rpath,\\\$\$ORIGIN/lib,-rpath,\\\$\$ORIGIN/lib/HCNetSDKCom"
    make -j"$(nproc)"
    # .pro file sets TARGET=../../../QtClientDemo for in-source builds; in a shadow
    # build from $out the binary lands outside $out — move it in.
    [ -f ../../../QtClientDemo ] && mv ../../../QtClientDemo "$out/QtClientDemo"
    cp -r "$SDK/lib" "$out/"
    cp -r "$SDK/QtDemo/translation" "$out/"

    # libPlayCtrl.so ships with RPATH="./", which resolves to the process's
    # working directory, not the library's own directory.  Fix it to $ORIGIN
    # so dlopen can find libAudioRender.so / libSuperRender.so as siblings.
    if command -v patchelf >/dev/null 2>&1; then
        patchelf --set-rpath '$ORIGIN' "$out/lib/libPlayCtrl.so"
        echo "[patch] Fixed libPlayCtrl.so RPATH → \$ORIGIN"
    else
        echo "[warn] patchelf not found — install it to fix libPlayCtrl.so RPATH:"
        echo "       sudo apt-get install -y patchelf"
        echo "       patchelf --set-rpath '\$ORIGIN' $out/lib/libPlayCtrl.so"
    fi

    cat > "$out/run.sh" << 'EOF'
#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:$DIR/lib/HCNetSDKCom${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# On KVM/VNC hosts there is no hardware GPU, so libSuperRender.so (which uses
# OpenGL/GLX internally) crashes when the first video frame arrives.  Detect
# this and fall back to Mesa software rendering automatically.
# Override by setting HK_FORCE_SW_RENDER=0 (skip) or =1 (force) explicitly.
_use_sw_render="${HK_FORCE_SW_RENDER:-}"
if [ -z "$_use_sw_render" ]; then
    # systemd-detect-virt exits 0 and prints a non-"none" token when in a VM
    _virt="$(systemd-detect-virt 2>/dev/null || true)"
    if [ -n "$_virt" ] && [ "$_virt" != "none" ]; then
        _use_sw_render=1
    else
        _use_sw_render=0
    fi
fi
if [ "$_use_sw_render" = "1" ]; then
    export LIBGL_ALWAYS_SOFTWARE=1
    export QT_X11_NO_MITSHM=1
fi
unset _use_sw_render _virt

exec "$DIR/QtClientDemo" "$@"
EOF
    chmod +x "$out/run.sh"
    echo "→ Output: $out/QtClientDemo  (run via $out/run.sh)"
}

case "$TARGET" in
    console) build_console ;;
    qt)      build_qt ;;
    all)     build_console; build_qt ;;
    *)       echo "Usage: $0 [qt|console|all]"; exit 1 ;;
esac

echo "Done."
