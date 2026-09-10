#pragma once

#include <QObject>
#include <QString>
#include <cstdint>

class AudioGraph;
class LinkController;
class VirtualDeviceManager;
class AutoReconnectManager;
struct AudioNode;

// Verhindert, dass abspielende Apps (z.B. Spotify) pausieren, sobald der
// Nutzer in MixPipe alle ihre Verbindungen kappt: PipeWire-Playback-Streams
// ohne jeden verbundenen Port bekommen keinen Treiber/Takt mehr zugeteilt,
// woraufhin viele Apps (Spotify, Browser, ...) das als "kein Ausgabegerät
// mehr vorhanden" interpretieren und selbst pausieren.
//
// Lösung: ein für den Nutzer unsichtbares, stummes Loopback-Sink (siehe
// VirtualDeviceManager::ensureFallbackDevice) wird an jede in MixPipe
// angeheftete Quell-App zusätzlich zu deren realen Verbindungen dauerhaft
// mitverlinkt. Die App hat dadurch immer mindestens einen aktiven Port, ganz
// gleich was der Nutzer an echten Verbindungen herstellt oder löst - ohne
// dass davon irgendwo hörbarer Ton ankommt (das Loopback-Source-Ende wird nie
// irgendwohin weiterverbunden). Die Fallback-Verbindung selbst bleibt in der
// UI unsichtbar (Zielknoten ist AudioNode::isInternal) und wird nicht in der
// Session gespeichert (LinkController::createLinkSilent).
class SilentFallbackManager : public QObject {
    Q_OBJECT

public:
    SilentFallbackManager(AudioGraph *graph, LinkController *linkController,
                           VirtualDeviceManager *virtualDevices,
                           AutoReconnectManager *autoReconnect, QObject *parent = nullptr);

    // Legt das interne Fallback-Gerät an. Erst nach erfolgreichem
    // PipeWireEngine::start() aufrufen (siehe MainWindow).
    void start();

    // Für angeheftete Quell-Apps, deren einzige Verbindung die (stumme)
    // Fallback-Verbindung ist: vor der Zerstörung des Fallback-Geräts beim
    // Beenden von MixPipe zusätzlich auf das echte Standard-Ausgabegerät
    // verlinken, damit nicht einfach Stille übrig bleibt (analog
    // AutoReconnectManager::restoreDefaultRoutingBeforeShutdown). Vor der
    // Zerstörung der virtuellen Geräte aufrufen (MainWindow::~MainWindow).
    void beforeShutdown(const QString &defaultSinkName);

private:
    void ensureFallbackLink(const AudioNode &node);
    void ensureFallbackForAllPinnedSourceApps();

    AudioGraph *m_graph;
    LinkController *m_linkController;
    VirtualDeviceManager *m_virtualDevices;
    AutoReconnectManager *m_autoReconnect;

    // id des "_sink"-Endes (Audio/Sink, das ist wo Apps reinspielen) des
    // internen Fallback-Loopbacks, sobald es im Graphen aufgetaucht ist - 0
    // solange noch unbekannt.
    uint32_t m_fallbackSinkNodeId = 0;
};
