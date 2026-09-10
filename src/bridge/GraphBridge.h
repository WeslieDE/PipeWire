#pragma once

#include <QMap>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class AudioGraph;
class LinkController;
class VolumeController;
class VirtualDeviceManager;
class AutoReconnectManager;
struct AudioNode;
struct AudioLink;

// Einziger Berührungspunkt zwischen C++-Backend und der Web-UI (QWebChannel).
// Reicht Anfragen von JS an die passenden Controller durch und spiegelt
// AudioGraph-/Controller-Signale als schlanke QVariantMap-Events an JS
// zurück - die Aufruf-Signatur entspricht bewusst dem mock.js-Stub aus
// Meilenstein 6, damit web/app.js unverändert bleibt.
class GraphBridge : public QObject {
    Q_OBJECT

public:
    GraphBridge(AudioGraph *graph, LinkController *linkController,
                VolumeController *volumeController, VirtualDeviceManager *virtualDevices,
                AutoReconnectManager *autoReconnect, QObject *parent = nullptr);

    // Nur angeheftete (oder virtuelle) Nodes - Sichtbarkeit ist Opt-in, siehe
    // pinNode/unpinNode.
    Q_INVOKABLE QVariantList getNodes() const;
    Q_INVOKABLE QVariantList getLinks() const;

    // Nicht angeheftete Nodes der passenden Seite ("source"/"sink"),
    // Kandidaten fürs "Add"-Dropdown.
    Q_INVOKABLE QVariantList getAvailableNodes(const QString &side) const;

    Q_INVOKABLE void createLink(quint32 outputNodeId, quint32 inputNodeId);
    Q_INVOKABLE void removeLink(quint32 outputNodeId, quint32 inputNodeId);
    Q_INVOKABLE void setVolume(quint32 nodeId, double linearVolume);
    Q_INVOKABLE void setMute(quint32 nodeId, bool muted);
    Q_INVOKABLE QString createVirtualDevice(const QString &displayName);
    // nodeId: die id EINER Hälfte (Sink- oder Source-Node) des virtuellen
    // Geräts - web/app.js kennt (wie beim echten Node) nur eine einzelne
    // Karten-id, nicht den internen VirtualDeviceManager-Handle-String.
    Q_INVOKABLE void removeVirtualDevice(quint32 nodeId);

    // Sichtbarkeits-Kuration: reale Nodes sind erst nach explizitem Anheften
    // sichtbar. Virtuelle Geräte sind immer sichtbar (Entfernen = Löschen,
    // siehe removeVirtualDevice).
    Q_INVOKABLE void pinNode(quint32 nodeId);
    Q_INVOKABLE void unpinNode(quint32 nodeId);

signals:
    void nodeAdded(QVariantMap node);
    void nodeRemoved(quint32 id);
    void linkAdded(QVariantMap link);
    void linkRemoved(QVariantMap link);
    void volumeChanged(QVariantMap payload);

private:
    QVariantMap toVariant(const AudioNode &node) const;
    QVariantMap toVariant(const AudioLink &link) const;
    // Verweist ein Link-Ende auf das interne Silent-Fallback-Gerät (siehe
    // SilentFallbackManager)? Solche Links sind ein reines
    // Implementierungsdetail und dürfen nie an die UI durchgereicht werden.
    bool isInternalLink(const AudioLink &link) const;

    AudioGraph *m_graph;
    LinkController *m_linkController;
    VolumeController *m_volumeController;
    VirtualDeviceManager *m_virtualDevices;
    AutoReconnectManager *m_autoReconnect;

    // GraphBridge ist die einzige Stelle, die "aktuelle Lautstärke pro Node"
    // für die UI vorhält (weder AudioGraph noch VolumeController tun das -
    // PipeWire-Volume-Änderungen werden fire-and-forget gesendet, nicht
    // zurückgelesen). Optimistisch aus den eigenen setVolume/setMute-Aufrufen
    // sowie aus VolumeController::volumeChanged/muteChanged gespeist (letzteres
    // deckt auch von AutoReconnectManager wiederhergestellte Werte ab).
    QMap<quint32, double> m_volumes;
    QMap<quint32, bool> m_muted;
};
