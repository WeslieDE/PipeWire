# MixPipe

Ein einfacher, node-basierter Audio-Router für Linux/PipeWire — inspiriert von "MixLine"
(Windows), aber kein 1:1-Nachbau. Verbindet beliebige Sound-Quellen (abspielende Apps,
physische Mikrofone) mit beliebigen Zielen (Lautsprecher, aufnehmende Apps, virtuelle
Geräte) per Drag-and-Drop zwischen Nodes.

Status: in aktiver Entwicklung (Scaffold-Phase).

## Build

Voraussetzungen: Qt 6 (inkl. WebEngineWidgets, WebChannel), CMake ≥ 3.21, PipeWire-
Entwicklungspakete (`libpipewire-0.3`), ein C++20-fähiger Compiler.

```sh
cmake -B build -S .
cmake --build build
./build/mixpipe
```

Weitere Details folgen mit dem Fortschritt der Umsetzung.
