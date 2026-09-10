#pragma once

#include <QObject>
#include <cstdint>

class PipeWireEngine;
class AudioGraph;

// Erzeugt/löst Verbindungen (pw_link) zwischen zwei Nodes. Verbindungen werden
// auf Node-Ebene angefragt (ohne explizite Port-IDs) - PipeWires eigene
// Link-Factory verhandelt dabei selbstständig die passenden Kanäle (z.B. 2
// pw_link-Objekte für Stereo). Für die MixPipe-UI zählt das weiterhin als
// eine logische Verbindung zwischen zwei Node-Karten.
class LinkController : public QObject {
    Q_OBJECT

public:
    explicit LinkController(PipeWireEngine *engine, QObject *parent = nullptr);

    void createLink(uint32_t outputNodeId, uint32_t inputNodeId);

    // Löst genau den einen pw_link mit dieser globalen id.
    void removeLink(uint32_t linkId);

    // Löst alle pw_link-Objekte zwischen den beiden Nodes (z.B. beide
    // Stereo-Kanäle einer Verbindung).
    void removeNodeLink(AudioGraph *graph, uint32_t outputNodeId, uint32_t inputNodeId);

private:
    PipeWireEngine *m_engine;
};
