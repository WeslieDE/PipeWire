#include "PipeWireEngine.h"

#include "AudioGraph.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

#include <cstdio>

#include <pipewire/extensions/metadata.h>

// Wichtig: In diesem Modul (insb. in den statischen Callbacks, die vom
// PipeWire-eigenen pw_thread_loop-Thread aus aufgerufen werden) bewusst KEIN
// qDebug()/qWarning() verwenden. In Tests hat sich gezeigt, dass Qt's
// Logging-Machinerie beim allerersten Aufruf aus diesem "fremden", nicht von
// Qt verwalteten Thread hängen bleibt und dadurch die komplette
// Registry-Event-Verarbeitung zum Stillstand bringt. Für Diagnose-Ausgaben
// aus diesem Kontext daher fprintf(stderr, ...) verwenden.

namespace {

NodeRole classifyMediaClass(const QString &mediaClass)
{
    if (mediaClass == QLatin1String("Stream/Output/Audio")) {
        return NodeRole::SourceApp;
    }
    if (mediaClass == QLatin1String("Audio/Source")) {
        return NodeRole::SourceDevice;
    }
    if (mediaClass == QLatin1String("Audio/Sink")) {
        return NodeRole::SinkDevice;
    }
    if (mediaClass == QLatin1String("Stream/Input/Audio")) {
        return NodeRole::SinkApp;
    }
    return NodeRole::Unknown;
}

QString dictValue(const struct spa_dict *props, const char *key)
{
    if (!props) {
        return {};
    }
    const char *value = spa_dict_lookup(props, key);
    return value ? QString::fromUtf8(value) : QString();
}

} // namespace

PipeWireEngine::PipeWireEngine(AudioGraph *graph, QObject *parent)
    : QObject(parent)
    , m_graph(graph)
{
}

PipeWireEngine::~PipeWireEngine()
{
    stop();
}

bool PipeWireEngine::start()
{
    if (m_running) {
        return true;
    }

    pw_init(nullptr, nullptr);

    m_loop = pw_thread_loop_new("mixpipe-pipewire", nullptr);
    if (!m_loop) {
        fprintf(stderr, "PipeWireEngine: pw_thread_loop_new fehlgeschlagen\n");
        pw_deinit();
        return false;
    }

    // Lock zuerst, dann starten: der Thread läuft zwar schon los, blockiert
    // aber beim Versuch, für seine erste Iteration denselben Lock zu holen,
    // bis wir unten unlocken. So ist das Setup unten atomar gegenüber dem
    // neuen Thread.
    pw_thread_loop_lock(m_loop);
    pw_thread_loop_start(m_loop);

    m_context = pw_context_new(pw_thread_loop_get_loop(m_loop), nullptr, 0);
    if (!m_context) {
        fprintf(stderr, "PipeWireEngine: pw_context_new fehlgeschlagen\n");
        pw_thread_loop_unlock(m_loop);
        pw_thread_loop_stop(m_loop);
        pw_thread_loop_destroy(m_loop);
        m_loop = nullptr;
        pw_deinit();
        return false;
    }

    m_core = pw_context_connect(m_context, nullptr, 0);
    if (!m_core) {
        fprintf(stderr, "PipeWireEngine: keine Verbindung zum PipeWire-Daemon (läuft er?)\n");
        pw_thread_loop_unlock(m_loop);
        pw_thread_loop_stop(m_loop);
        pw_thread_loop_destroy(m_loop);
        m_loop = nullptr;
        pw_context_destroy(m_context);
        m_context = nullptr;
        pw_deinit();
        return false;
    }

    // Fehler vom Server (z.B. eine fehlgeschlagene Link-Erstellung) landen sonst
    // nirgends; bis GraphBridge (M7) sie an die UI weiterreicht, wenigstens auf
    // stderr sichtbar machen.
    static struct spa_hook coreErrorListener {};
    static const struct pw_core_events coreEvents = {
        .version = PW_VERSION_CORE_EVENTS,
        .error = [](void *, uint32_t id, int seq, int res, const char *message) {
            fprintf(stderr, "PipeWireEngine: core error id=%u seq=%d res=%d msg=%s\n", id, seq,
                    res, message);
        },
    };
    spa_zero(coreErrorListener);
    pw_core_add_listener(m_core, &coreErrorListener, &coreEvents, this);

    m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0);

    static const struct pw_registry_events registryEvents = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = &PipeWireEngine::onGlobalAdded,
        .global_remove = &PipeWireEngine::onGlobalRemoved,
    };

    spa_zero(m_registryListener);
    pw_registry_add_listener(m_registry, &m_registryListener, &registryEvents, this);

    pw_thread_loop_unlock(m_loop);

    m_running = true;
    return true;
}

void PipeWireEngine::stop()
{
    if (!m_running && !m_loop) {
        return;
    }

    if (m_loop) {
        pw_thread_loop_stop(m_loop);
    }

    if (m_defaultMetadata) {
        pw_proxy_destroy(reinterpret_cast<struct pw_proxy *>(m_defaultMetadata));
        m_defaultMetadata = nullptr;
    }
    if (m_registry) {
        pw_proxy_destroy(reinterpret_cast<struct pw_proxy *>(m_registry));
        m_registry = nullptr;
    }
    if (m_core) {
        pw_core_disconnect(m_core);
        m_core = nullptr;
    }
    if (m_context) {
        pw_context_destroy(m_context);
        m_context = nullptr;
    }
    if (m_loop) {
        pw_thread_loop_destroy(m_loop);
        m_loop = nullptr;
    }

    pw_deinit();

    m_running = false;
}

void PipeWireEngine::runLocked(const std::function<void()> &fn)
{
    if (!m_loop) {
        return;
    }
    pw_thread_loop_lock(m_loop);
    fn();
    pw_thread_loop_unlock(m_loop);
}

void PipeWireEngine::onGlobalAdded(void *data, uint32_t id, uint32_t /*permissions*/,
                                    const char *type, uint32_t /*version*/,
                                    const struct spa_dict *props)
{
    auto *self = static_cast<PipeWireEngine *>(data);

    const QString typeStr = QString::fromUtf8(type);
    if (typeStr == QLatin1String(PW_TYPE_INTERFACE_Node)) {
        self->handleNodeGlobal(id, props);
    } else if (typeStr == QLatin1String(PW_TYPE_INTERFACE_Port)) {
        self->handlePortGlobal(id, props);
    } else if (typeStr == QLatin1String(PW_TYPE_INTERFACE_Link)) {
        self->handleLinkGlobal(id, props);
    } else if (typeStr == QLatin1String(PW_TYPE_INTERFACE_Metadata)) {
        self->handleMetadataGlobal(id, props);
    }
}

void PipeWireEngine::onGlobalRemoved(void *data, uint32_t id)
{
    auto *self = static_cast<PipeWireEngine *>(data);
    AudioGraph *graph = self->m_graph;

    // Wir wissen an dieser Stelle nicht mehr, ob die id ein Node/Port/Link war;
    // AudioGraph::removeNode/removePort/removeLink sind No-Ops, wenn die id dort
    // nicht bekannt ist, daher können wir gefahrlos alle drei versuchen.
    QMetaObject::invokeMethod(
        graph,
        [graph, id]() {
            graph->removeNode(id);
            graph->removePort(id);
            graph->removeLink(id);
        },
        Qt::QueuedConnection);
}

void PipeWireEngine::handleNodeGlobal(uint32_t id, const struct spa_dict *props)
{
    AudioNode node;
    node.id = id;
    node.role = classifyMediaClass(dictValue(props, PW_KEY_MEDIA_CLASS));
    node.name = dictValue(props, PW_KEY_NODE_NAME);

    QString description = dictValue(props, PW_KEY_NODE_DESCRIPTION);
    if (description.isEmpty()) {
        description = dictValue(props, PW_KEY_DEVICE_DESCRIPTION);
    }
    if (description.isEmpty()) {
        description = node.name;
    }
    node.description = description;

    node.appName = dictValue(props, PW_KEY_APP_NAME);
    node.processBinary = dictValue(props, PW_KEY_APP_PROCESS_BINARY);
    node.iconName = dictValue(props, PW_KEY_APP_ICON_NAME);
    node.isVirtual = node.name.startsWith(QLatin1String(kVirtualNodeNamePrefix));

    if (node.role == NodeRole::Unknown) {
        // Kein Audio-Node (z.B. Video), für MixPipe irrelevant.
        return;
    }

    AudioGraph *graph = m_graph;
    QMetaObject::invokeMethod(
        graph, [graph, node]() { graph->upsertNode(node); }, Qt::QueuedConnection);
}

void PipeWireEngine::handlePortGlobal(uint32_t id, const struct spa_dict *props)
{
    AudioPort port;
    port.id = id;
    port.name = dictValue(props, PW_KEY_PORT_NAME);
    port.isInput = dictValue(props, PW_KEY_PORT_DIRECTION) == QLatin1String("in");
    port.channel = dictValue(props, PW_KEY_AUDIO_CHANNEL);

    const QString nodeIdStr = dictValue(props, PW_KEY_NODE_ID);
    bool ok = false;
    port.nodeId = nodeIdStr.toUInt(&ok);
    if (!ok) {
        return;
    }

    AudioGraph *graph = m_graph;
    QMetaObject::invokeMethod(
        graph, [graph, port]() { graph->upsertPort(port); }, Qt::QueuedConnection);
}

void PipeWireEngine::handleLinkGlobal(uint32_t id, const struct spa_dict *props)
{
    AudioLink link;
    link.id = id;

    bool ok = false;
    link.outputNodeId = dictValue(props, PW_KEY_LINK_OUTPUT_NODE).toUInt(&ok);
    if (!ok) {
        return;
    }
    link.outputPortId = dictValue(props, PW_KEY_LINK_OUTPUT_PORT).toUInt(&ok);
    if (!ok) {
        return;
    }
    link.inputNodeId = dictValue(props, PW_KEY_LINK_INPUT_NODE).toUInt(&ok);
    if (!ok) {
        return;
    }
    link.inputPortId = dictValue(props, PW_KEY_LINK_INPUT_PORT).toUInt(&ok);
    if (!ok) {
        return;
    }

    AudioGraph *graph = m_graph;
    QMetaObject::invokeMethod(
        graph, [graph, link]() { graph->upsertLink(link); }, Qt::QueuedConnection);
}

void PipeWireEngine::handleMetadataGlobal(uint32_t id, const struct spa_dict *props)
{
    // Es gibt mehrere Metadata-Objekte (default, settings, route-settings,
    // ...) - uns interessiert nur das mit metadata.name == "default", das
    // hält u.a. default.audio.sink/-source.
    if (dictValue(props, PW_KEY_METADATA_NAME) != QLatin1String("default")) {
        return;
    }

    m_defaultMetadata = static_cast<struct pw_metadata *>(
        pw_registry_bind(m_registry, id, PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, 0));
    if (!m_defaultMetadata) {
        return;
    }

    static const struct pw_metadata_events metadataEvents = {
        .version = PW_VERSION_METADATA_EVENTS,
        .property = &PipeWireEngine::onDefaultMetadataProperty,
    };
    spa_zero(m_defaultMetadataListener);
    pw_metadata_add_listener(m_defaultMetadata, &m_defaultMetadataListener, &metadataEvents,
                              this);
}

int PipeWireEngine::onDefaultMetadataProperty(void *data, uint32_t /*subject*/, const char *key,
                                               const char * /*type*/, const char *value)
{
    if (!key || !value) {
        return 0;
    }
    const QString keyStr = QString::fromUtf8(key);
    const bool isSink = keyStr == QLatin1String("default.audio.sink");
    const bool isSource = keyStr == QLatin1String("default.audio.source");
    if (!isSink && !isSource) {
        return 0;
    }

    auto *self = static_cast<PipeWireEngine *>(data);
    const QByteArray valueBytes(value);

    QMetaObject::invokeMethod(
        self,
        [self, valueBytes, isSink]() {
            const QJsonDocument doc = QJsonDocument::fromJson(valueBytes);
            const QString nodeName = doc.object().value(QStringLiteral("name")).toString();
            if (isSink) {
                self->m_defaultSinkName = nodeName;
                emit self->defaultSinkChanged(nodeName);
            } else {
                self->m_defaultSourceName = nodeName;
                emit self->defaultSourceChanged(nodeName);
            }
        },
        Qt::QueuedConnection);

    return 0;
}
