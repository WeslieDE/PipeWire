# Build-Umgebung für das MixPipe-AppImage. Bewusst Debian 12 (glibc 2.36) statt
# des Host-Systems (CachyOS, glibc 2.44): AppImages sind nur rückwärtskompatibel
# zu neueren glibc-Versionen, nicht vorwärts - ein auf CachyOS gebautes AppImage
# liefe auf älteren/stabilen Distros (Ubuntu 22.04, Debian 12 selbst, ...) nicht
# zuverlässig. Debian 12 ist ein guter Kompromiss: alt genug für breite
# Kompatibilität, aktuell genug für Qt 6.4 + PipeWire 0.3.65.
#
# Dieses Image enthält nur die Build-Werkzeuge, nicht den MixPipe-Quellcode
# selbst - der wird beim `docker run` als Volume eingehängt (siehe
# build-appimage.sh), damit für jede Code-Änderung nicht das ganze Image neu
# gebaut werden muss.
FROM debian:12

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    qt6-base-dev \
    qt6-webengine-dev \
    qt6-webengine-dev-tools \
    qt6-webchannel-dev \
    libpipewire-0.3-dev \
    libpipewire-0.3-modules \
    libspa-0.2-modules \
    wget \
    ca-certificates \
    file \
    patchelf \
    desktop-file-utils \
    librsvg2-bin \
    && rm -rf /var/lib/apt/lists/*

# linuxdeploy + Qt-Plugin + appimagetool als Continuous-Build-AppImages laden
# und sofort extrahieren (--appimage-extract), da FUSE in Docker-Containern
# i.d.R. nicht verfügbar ist.
WORKDIR /opt/linuxdeploy
RUN wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
    && wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
    && wget -q https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage \
    && chmod +x ./*.AppImage \
    && ./linuxdeploy-x86_64.AppImage --appimage-extract >/dev/null \
    && mv squashfs-root linuxdeploy.dir \
    && ./linuxdeploy-plugin-qt-x86_64.AppImage --appimage-extract >/dev/null \
    && mv squashfs-root linuxdeploy-plugin-qt.dir \
    && ./appimagetool-x86_64.AppImage --appimage-extract >/dev/null \
    && mv squashfs-root appimagetool.dir \
    && rm -f ./*.AppImage

ENV PATH="/opt/linuxdeploy/linuxdeploy-plugin-qt.dir/usr/bin:/opt/linuxdeploy/linuxdeploy.dir/usr/bin:/opt/linuxdeploy/appimagetool.dir/usr/bin:${PATH}"

WORKDIR /src
