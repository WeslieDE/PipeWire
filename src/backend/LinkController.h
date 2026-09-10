#pragma once

#include <QObject>
#include <cstdint>

class PipeWireEngine;
class AudioGraph;

// Erzeugt/löst Verbindungen (pw_link) zwischen zwei Nodes. Verbindet dafür
// deren Ports paarweise explizit (in Reihenfolge, z.B. beide Stereo-Kanäle) -
// PipeWires eigene Link-Factory kann zwar auch rein über Node-IDs
// auto-verhandeln, scheitert dabei aber, sobald der Ausgabe-Node schon anderweitig
// verbunden ist (z.B. bereits an die Standard-Ausgabe angeschlossene App
// zusätzlich in ein virtuelles Gerät routen - genau der MixLine-Kernfall).
// Für die MixPipe-UI zählen alle so entstandenen pw_link-Objekte weiterhin als
// eine logische Verbindung zwischen zwei Node-Karten.
class LinkController : public QObject {
    Q_OBJECT

public:
    LinkController(PipeWireEngine *engine, AudioGraph *graph, QObject *parent = nullptr);

    void createLink(uint32_t outputNodeId, uint32_t inputNodeId);

    // Löst genau den einen pw_link mit dieser globalen id.
    void removeLink(uint32_t linkId);

    // Löst alle pw_link-Objekte zwischen den beiden Nodes (z.B. beide
    // Stereo-Kanäle einer Verbindung).
    void removeNodeLink(uint32_t outputNodeId, uint32_t inputNodeId);

signals:
    // Für AutoReconnectManager: nur MixPipe-initiierte Verbindungen sollen
    // persistiert werden, nicht jede Verbindung, die zufällig im System-Graph
    // auftaucht (z.B. WirePlumbers eigenes Default-Routing).
    void linkRequested(uint32_t outputNodeId, uint32_t inputNodeId);

private:
    PipeWireEngine *m_engine;
    AudioGraph *m_graph;
};
