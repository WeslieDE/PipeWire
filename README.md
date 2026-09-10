# MixPipe

Ein einfacher, node-basierter Audio-Router für Linux/PipeWire — inspiriert von "MixLine"
(Windows), aber kein 1:1-Nachbau. Verbindet beliebige Sound-Quellen (abspielende Apps,
physische Mikrofone) mit beliebigen Zielen (Lautsprecher, aufnehmende Apps, virtuelle
Geräte) per Drag-and-Drop zwischen Nodes — Verbindungen als Bezier-Linien, mehrfache
Verbindungen pro Node möglich, Lautstärke/Mute pro Node, virtuelle Geräte lassen sich
direkt aus der UI anlegen.

Läuft als portables AppImage; ohne laufendes AppImage funktioniert das restliche System
normal weiter (virtuelle Geräte/Loopbacks sind an die Lebenszeit der GUI gebunden).
Verbindungen, virtuelle Geräte und Lautstärken werden aber sitzungsübergreifend gespeichert
und beim nächsten Start automatisch wiederhergestellt, sobald die passenden Apps/Geräte
wieder auftauchen.

## Architektur

- **Backend** (`src/backend/`): eigenständiger PipeWire-Client (`PipeWireEngine`) über
  `libpipewire-0.3`, beobachtet den System-Audio-Graph per Registry-Listener
  (`AudioGraph`), erstellt/löst Verbindungen (`LinkController`), setzt Lautstärke/Mute
  (`VolumeController`), legt virtuelle Geräte an (`VirtualDeviceManager` via
  `libpipewire-module-loopback`) und persistiert/reaktiviert die Konfiguration
  sitzungsübergreifend (`SessionStore` + `AutoReconnectManager`).
- **Bridge** (`src/bridge/`): `GraphBridge` exponiert das Backend per `QWebChannel` an die
  Web-UI.
- **UI** (`web/`): Vanilla HTML/CSS/JS (kein Framework), läuft in einer `QWebEngineView`.
  `qtbridge.js` verbindet sich mit der echten Bridge; `mock.js` ist ein Stand-in mit
  identischer Schnittstelle für reine Browser-Entwicklung ohne Qt (`index.html` einfach
  temporär auf `mock.js` statt `qtbridge.js`/`qwebchannel.js` umstellen).
- **UI-Schicht** (`src/ui/`): `MainWindow` baut den Backend-Stack auf und hostet die
  `QWebEngineView`.

## Native bauen (Entwicklung)

Voraussetzungen: Qt 6 (inkl. WebEngineWidgets, WebChannel), CMake ≥ 3.21, PipeWire-
Entwicklungspakete (`libpipewire-0.3`), ein C++20-fähiger Compiler.

```sh
cmake -B build -S .
cmake --build build
./build/mixpipe
```

## Portables AppImage bauen

```sh
packaging/build-appimage.sh
```

Baut komplett in einem Debian-12-Docker-Container (nicht nativ auf dem Host) — Debian 12s
glibc ist alt genug, um auf praktisch allen aktuell unterstützten Distros lauffähig zu sein;
ein auf einem sehr aktuellen Host (z.B. Arch-basiert) nativ gebautes AppImage wäre das
nicht, da glibc nur rückwärts-, nicht vorwärtskompatibel ist. Docker wird benötigt. Ergebnis:
`dist/MixPipe-x86_64.AppImage`.

Das Docker-Image (Qt6/PipeWire-Dev-Pakete + linuxdeploy-Werkzeuge) wird beim ersten Lauf
gebaut und danach von Docker gecacht — spätere Läufe nach Code-Änderungen sind deutlich
schneller.

## Status

In aktiver Entwicklung. Kernfunktionalität (Routing, virtuelle Geräte, Lautstärke,
Persistenz, UI, AppImage) steht und ist gegen den echten PipeWire-Daemon verifiziert.
