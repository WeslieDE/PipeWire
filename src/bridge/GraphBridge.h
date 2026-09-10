#pragma once

#include <QMap>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class AudioGraph;
class LinkController;
class VolumeController;
class VirtualDeviceManager;
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
                QObject *parent = nullptr);

    Q_INVOKABLE QVariantList getNodes() const;
    Q_INVOKABLE QVariantList getLinks() const;

    // Curation-Feature aus dem M6-Mock (einzelne Nodes ein-/ausblenden) ist
    // in dieser Version nicht umgesetzt - alle erkannten Nodes sind immer
    // sichtbar. Bleibt als Methode bestehen, damit web/app.js' "Add"-Dropdown
    // ohne Änderung funktioniert (liefert aktuell immer eine leere Liste).
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

signals:
    void nodeAdded(QVariantMap node);
    void nodeRemoved(quint32 id);
    void linkAdded(QVariantMap link);
    void linkRemoved(QVariantMap link);
    void volumeChanged(QVariantMap payload);

private:
    QVariantMap toVariant(const AudioNode &node) const;
    QVariantMap toVariant(const AudioLink &link) const;

    AudioGraph *m_graph;
    LinkController *m_linkController;
    VolumeController *m_volumeController;
    VirtualDeviceManager *m_virtualDevices;

    // GraphBridge ist die einzige Stelle, die "aktuelle Lautstärke pro Node"
    // für die UI vorhält (weder AudioGraph noch VolumeController tun das -
    // PipeWire-Volume-Änderungen werden fire-and-forget gesendet, nicht
    // zurückgelesen). Optimistisch aus den eigenen setVolume/setMute-Aufrufen
    // sowie aus VolumeController::volumeChanged/muteChanged gespeist (letzteres
    // deckt auch von AutoReconnectManager wiederhergestellte Werte ab).
    QMap<quint32, double> m_volumes;
    QMap<quint32, bool> m_muted;
};
