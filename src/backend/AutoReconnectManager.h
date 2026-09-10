#pragma once

#include "SessionStore.h"

#include <QObject>
#include <QPair>
#include <QSet>
#include <QTimer>
#include <cstdint>

class AudioGraph;
class LinkController;
class VolumeController;
class VirtualDeviceManager;
struct AudioNode;
struct AudioPort;
struct AudioLink;

// Setzt eine gespeicherte Session beim Start um (virtuelle Geräte sofort neu
// anlegen, Verbindungen/Lautstärken reaktivieren sobald passende Nodes im
// Graphen auftauchen - unabhängig davon, ob die zugehörige App schon beim
// MixPipe-Start lief oder erst später startet) und schreibt Änderungen am
// laufenden Graphen (debounced) zurück in die Session.
class AutoReconnectManager : public QObject {
    Q_OBJECT

public:
    AutoReconnectManager(AudioGraph *graph, LinkController *linkController,
                          VolumeController *volumeController,
                          VirtualDeviceManager *virtualDevices, QObject *parent = nullptr);

    // Legt gespeicherte virtuelle Geräte an und merkt sich die gespeicherten
    // Regeln zur fortlaufenden Reaktivierung. Erst nach erfolgreichem
    // PipeWireEngine::start() aufrufen.
    void restoreSession();

private slots:
    void onNodeAdded(const AudioNode &node);
    void onPortAdded(const AudioPort &port);
    void onLinkRequested(uint32_t outputNodeId, uint32_t inputNodeId);
    void onLinkRemoved(const AudioLink &link);
    void onNodeRemoved(uint32_t id);

private:
    void applyRulesFor(const AudioNode &node);
    void scheduleSave();
    void saveNow();

    AudioGraph *m_graph;
    LinkController *m_linkController;
    VolumeController *m_volumeController;
    VirtualDeviceManager *m_virtualDevices;

    SessionStore m_store;
    // handleId -> displayName, in Erstellungsreihenfolge (wichtig: bestimmt
    // die Handle-IDs, die VirtualDeviceManager beim nächsten Start erneut
    // vergibt).
    QList<QPair<QString, QString>> m_virtualDeviceEntries;
    QList<SessionStore::LinkRule> m_linkRules; // aus der Session geladen, zum Reaktivieren
    QList<SessionStore::NodeVolume> m_nodeVolumes;

    // Von MixPipe selbst (Nutzeraktion oder Reaktivierung) hergestellte
    // Verbindungen dieser Sitzung - nur diese werden gespeichert, nicht jede
    // im System-Graph sichtbare Verbindung.
    QSet<QPair<uint32_t, uint32_t>> m_managedPairs;

    QTimer m_saveTimer;
};
