#!/usr/bin/env bash
# Läuft INNERHALB des mixpipe-builder-Containers (siehe build-appimage.sh).
# /src ist das per Volume eingehängte Projekt-Repo.
set -euo pipefail

cd /src

BUILD_DIR=build-appimage
APPDIR=packaging/AppDir
DIST_DIR=dist

rm -rf "$BUILD_DIR" "$APPDIR" "$DIST_DIR"
mkdir -p "$DIST_DIR"

echo "==> CMake configure + build"
cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Icon rendern"
# Dateiname muss zum Icon=-Eintrag im .desktop-File passen (linuxdeploy
# benennt nicht um).
rsvg-convert -w 256 -h 256 packaging/icon.svg -o packaging/mixpipe.png

echo "==> linuxdeploy (inkl. Qt-Plugin) - baut AppDir"
export QML_SOURCES_PATHS=/src
export LINUXDEPLOY_OUTPUT_VERSION="0.1.0"
linuxdeploy \
    --appdir "$APPDIR" \
    --executable "$BUILD_DIR/mixpipe" \
    --desktop-file packaging/mixpipe.desktop \
    --icon-file packaging/mixpipe.png \
    --library /lib/x86_64-linux-gnu/libpipewire-0.3.so.0 \
    --plugin qt

echo "==> PipeWire-Client-Module/SPA-Plugins bündeln (dlopen zur Laufzeit, von"
echo "    linuxdeploys ldd-basiertem Scan nicht automatisch erkannt)"
PW_MODULE_DIR=$(pkg-config --variable=moduledir libpipewire-0.3 2>/dev/null || true)
if [ -z "$PW_MODULE_DIR" ]; then
    PW_MODULE_DIR=/usr/lib/x86_64-linux-gnu/pipewire-0.3
fi
SPA_DIR=/usr/lib/x86_64-linux-gnu/spa-0.2

mkdir -p "$APPDIR/usr/lib/pipewire-0.3" "$APPDIR/usr/lib/spa-0.2"
cp -a "$PW_MODULE_DIR/." "$APPDIR/usr/lib/pipewire-0.3/"
cp -a "$SPA_DIR/." "$APPDIR/usr/lib/spa-0.2/"

echo "==> Eigenes AppRun (PipeWire-Umgebungsvariablen + Qt-Setup von linuxdeploy)"
mv "$APPDIR/AppRun" "$APPDIR/AppRun.qt"
cp packaging/AppRun "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"

echo "==> appimagetool - packt das finale AppImage"
ARCH=x86_64 appimagetool "$APPDIR" "$DIST_DIR/MixPipe-x86_64.AppImage"

echo "==> Fertig: $DIST_DIR/MixPipe-x86_64.AppImage"
