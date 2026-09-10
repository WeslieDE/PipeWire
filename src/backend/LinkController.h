#pragma once

#include <QObject>
#include <QPair>
#include <QSet>
#include <cstdint>

class PipeWireEngine;
class AudioGraph;
struct AudioPort;

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

    // Wie createLink(), aber ohne linkRequested-Signal - für vom
    // SilentFallbackManager intern hergestellte Ausweich-Verbindungen, die
    // NICHT in der Session persistiert werden sollen (siehe dort). Gibt
    // zurück, ob bei diesem Aufruf tatsächlich neue pw_link-Objekte
    // angefordert wurden (false z.B. wenn schon alle erwarteten Kanäle
    // verbunden sind, oder einer der Nodes noch keine Ports hat - im
    // letzteren Fall holt healPair() es automatisch nach, siehe unten).
    bool createLinkSilent(uint32_t outputNodeId, uint32_t inputNodeId);

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
    // true, wenn für dieses Paar tatsächlich noch fehlende Portpaare gefunden
    // und pw_link-Objekte angefordert wurden (false wenn schon vollständig
    // verbunden, oder wenn einer der Nodes aktuell keine passenden Ports hat).
    bool healPair(uint32_t outputNodeId, uint32_t inputNodeId);
    bool doCreateLink(uint32_t outputNodeId, uint32_t inputNodeId);

    PipeWireEngine *m_engine;
    AudioGraph *m_graph;

    // Node-Paare, die laut letztem createLink()/createLinkSilent()-Aufruf
    // verbunden bleiben sollen (durch removeNodeLink() wieder ausgetragen).
    // Manche Clients reißen beim (Ver-)Verbinden kurzzeitig ihre eigenen
    // Ports ab und legen sie neu an (Format-Renegotiation) - eine
    // Link-Anfrage, die genau in diesem Zeitfenster ankommt, sieht dann nur
    // einen unvollständigen Portsatz und verbindet z.B. nur den linken
    // Kanal, ohne dass je etwas nachbessert (Bugreport: "nach mehrmaligem
    // Trennen/Verbinden nur noch mono zu hören"). healPair() wird deshalb bei
    // jedem neu hinzukommenden Port eines beteiligten Nodes erneut versucht,
    // bis tatsächlich die volle erwartbare Kanalzahl verbunden ist.
    QSet<QPair<uint32_t, uint32_t>> m_desiredPairs;
};
