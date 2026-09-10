#!/usr/bin/env bash
# Baut MixPipe als portables AppImage. Läuft komplett in einem Debian-12-
# Docker-Container (siehe packaging/Dockerfile) statt nativ auf dem Host, damit
# das Ergebnis nicht an eine bestimmte (insb. sehr aktuelle) glibc-Version
# gebunden ist - AppImages sind nur rückwärts-, nicht vorwärtskompatibel zu
# neueren glibc-Versionen.
#
# Aufruf: packaging/build-appimage.sh
# Ergebnis: dist/MixPipe-x86_64.AppImage
#
# Docker-Image wird beim ersten Lauf gebaut (dauert ein paar Minuten wegen
# Qt6/PipeWire-Dev-Paketen) und danach von Docker gecacht - spätere Läufe
# nach Code-Änderungen sind deutlich schneller, da nur /src neu gemountet
# und der eigentliche C++-Build innerhalb des Containers erneut läuft.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_TAG="mixpipe-builder:debian12"

echo "==> Docker-Build-Image sicherstellen ($IMAGE_TAG)"
docker build -f "$SCRIPT_DIR/Dockerfile" -t "$IMAGE_TAG" "$REPO_ROOT"

echo "==> Build + Packaging im Container ausführen"
docker run --rm \
    -v "$REPO_ROOT:/src" \
    "$IMAGE_TAG" \
    bash /src/packaging/docker-build-inner.sh

# Container läuft als root; vom Container erzeugte Dateien gehören sonst root
# statt dem eigenen Nutzer. Passwordless sudo ist auf diesem System erlaubt.
echo "==> Besitzrechte der erzeugten Dateien korrigieren"
sudo chown -R "$(id -u):$(id -g)" \
    "$REPO_ROOT/dist" \
    "$REPO_ROOT/build-appimage" \
    "$REPO_ROOT/packaging/AppDir" \
    "$REPO_ROOT/packaging/mixpipe.png"

echo "==> Fertig: $REPO_ROOT/dist/MixPipe-x86_64.AppImage"
