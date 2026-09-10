#pragma once

#include <QList>
#include <QMap>
#include <QMetaType>
#include <QObject>
#include <QPair>
#include <QString>
#include <cstdint>
#include <optional>

// Links im Screenshot: abspielende Apps + physische Mikrofone.
// Rechts im Screenshot: Lautsprecher/Kopfhörer (inkl. virtuelle Sinks) + aufnehmende Apps.
enum class NodeRole {
    SourceApp,    // media.class == "Stream/Output/Audio"
    SourceDevice, // media.class == "Audio/Source"
    SinkDevice,   // media.class == "Audio/Sink"
    SinkApp,      // media.class == "Stream/Input/Audio"
    Unknown
};

bool isSourceRole(NodeRole role);
bool isSinkRole(NodeRole role);

// Namenspräfix für node.name virtueller, von MixPipe selbst angelegter
// Geräte (siehe VirtualDeviceManager). Wird von PipeWireEngine genutzt, um
// AudioNode::isVirtual zu setzen.
inline constexpr const char *kVirtualNodeNamePrefix = "mixpipe_virtual_";

// Namenspräfix des internen, unsichtbaren Silent-Fallback-Geräts (siehe
// SilentFallbackManager) - bewusst ein eigenes Präfix statt
// kVirtualNodeNamePrefix, damit dieses Gerät NICHT als AudioNode::isVirtual
// gilt und so nirgends in der UI (Node-Liste, "Hinzufügen"-Dropdown)
// auftaucht.
inline constexpr const char *kInternalNodeNamePrefix = "mixpipe_internal_";

struct AudioPort {
    uint32_t id = 0;
    uint32_t nodeId = 0;
    QString name;
    bool isInput = false;
    QString channel; // audio.channel, z.B. "FL"/"FR"/"MONO" - leer wenn unbekannt
};

struct AudioNode {
    uint32_t id = 0;
    NodeRole role = NodeRole::Unknown;
    QString name;        // node.name
    QString description; // node.description / device.description, Anzeigename
    QString appName;      // application.name, falls vorhanden (Apps statt Geräte)
    QString processBinary; // application.process.binary, stabiler als appName
    QString iconName;     // application.icon-name, falls vorhanden
    bool isVirtual = false; // von MixPipe selbst angelegt (VirtualDeviceManager)
    // Internes Implementierungsdetail (Silent-Fallback-Gerät, siehe
    // SilentFallbackManager) - nie in der UI anzeigen, nie anheftbar.
    bool isInternal = false;
};

// Sitzungsübergreifend stabile Identität eines Nodes (PipeWire-IDs wechseln
// pro Sitzung) - Grundlage für SessionStore/AutoReconnectManager, um
// gespeicherte Regeln nach einem Neustart wieder den richtigen Nodes
// zuzuordnen.
struct NodeIdentity {
    QString kind; // "app" | "device" | "virtual"
    QString key;

    bool isValid() const { return !kind.isEmpty() && !key.isEmpty(); }
    bool operator==(const NodeIdentity &other) const
    {
        return kind == other.kind && key == other.key;
    }
};

NodeIdentity identityForNode(const AudioNode &node);

struct AudioLink {
    uint32_t id = 0;
    uint32_t outputNodeId = 0;
    uint32_t outputPortId = 0;
    uint32_t inputNodeId = 0;
    uint32_t inputPortId = 0;
};

Q_DECLARE_METATYPE(AudioNode)
Q_DECLARE_METATYPE(AudioPort)
Q_DECLARE_METATYPE(AudioLink)

// Reines Datenmodell des Audio-Graphen. Wird ausschließlich vom Qt-Main-Thread aus
// mutiert (PipeWireEngine marshalt PipeWire-Thread-Events per Qt::QueuedConnection
// hierher), daher keine eigene Thread-Synchronisation nötig.
class AudioGraph : public QObject {
    Q_OBJECT

public:
    explicit AudioGraph(QObject *parent = nullptr);

    void upsertNode(const AudioNode &node);
    void removeNode(uint32_t id);

    void upsertPort(const AudioPort &port);
    void removePort(uint32_t id);

    void upsertLink(const AudioLink &link);
    void removeLink(uint32_t id);

    QList<AudioNode> nodes() const;
    QList<AudioLink> links() const;
    QList<AudioPort> portsForNode(uint32_t nodeId) const;
    std::optional<AudioNode> node(uint32_t id) const;
    std::optional<AudioPort> port(uint32_t id) const;

    // Eindeutige (outputNodeId, inputNodeId)-Paare aus den aktuellen Links -
    // eine PipeWire-Verbindung zwischen zwei Nodes kann aus mehreren
    // Port-Links bestehen (z.B. Stereo), zählt für die UI aber als eine
    // logische Verbindung.
    QList<QPair<uint32_t, uint32_t>> nodeLinkPairs() const;

    // Sucht den ersten aktuell bekannten Node mit passender Identität (siehe
    // identityForNode), oder nullopt.
    std::optional<AudioNode> findByIdentity(const NodeIdentity &identity) const;

signals:
    void nodeAdded(const AudioNode &node);
    void nodeUpdated(const AudioNode &node);
    void nodeRemoved(uint32_t id);
    void portAdded(const AudioPort &port);
    void linkAdded(const AudioLink &link);
    void linkRemoved(const AudioLink &link);

private:
    QMap<uint32_t, AudioNode> m_nodes;
    QMap<uint32_t, AudioPort> m_ports;
    QMap<uint32_t, AudioLink> m_links;
};
