#include "GraphBridge.h"

#include "backend/AudioGraph.h"
#include "backend/AutoReconnectManager.h"
#include "backend/LinkController.h"
#include "backend/VirtualDeviceManager.h"
#include "backend/VolumeController.h"

namespace {

QString roleString(NodeRole role)
{
    switch (role) {
    case NodeRole::SourceApp:
        return QStringLiteral("source-app");
    case NodeRole::SourceDevice:
        return QStringLiteral("source-device");
    case NodeRole::SinkDevice:
        return QStringLiteral("sink-device");
    case NodeRole::SinkApp:
        return QStringLiteral("sink-app");
    case NodeRole::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

bool roleMatchesSide(NodeRole role, const QString &side)
{
    if (side == QLatin1String("source")) {
        return isSourceRole(role);
    }
    if (side == QLatin1String("sink")) {
        return isSinkRole(role);
    }
    return false;
}

} // namespace

bool GraphBridge::isInternalLink(const AudioLink &link) const
{
    const auto outputNode = m_graph->node(link.outputNodeId);
    const auto inputNode = m_graph->node(link.inputNodeId);
    return (outputNode && outputNode->isInternal) || (inputNode && inputNode->isInternal);
}

GraphBridge::GraphBridge(AudioGraph *graph, LinkController *linkController,
                          VolumeController *volumeController,
                          VirtualDeviceManager *virtualDevices,
                          AutoReconnectManager *autoReconnect, QObject *parent)
    : QObject(parent)
    , m_graph(graph)
    , m_linkController(linkController)
    , m_volumeController(volumeController)
    , m_virtualDevices(virtualDevices)
    , m_autoReconnect(autoReconnect)
{
    connect(graph, &AudioGraph::nodeAdded, this, [this](const AudioNode &node) {
        if (node.isInternal) {
            return; // Implementierungsdetail (Silent-Fallback), nie in der UI zeigen.
        }
        if (node.isVirtual || m_autoReconnect->isPinned(identityForNode(node))) {
            emit nodeAdded(toVariant(node));
        }
    });
    connect(graph, &AudioGraph::nodeRemoved, this, [this](uint32_t id) {
        m_volumes.remove(id);
        m_muted.remove(id);
        emit nodeRemoved(id);
    });
    connect(graph, &AudioGraph::linkAdded, this, [this](const AudioLink &link) {
        if (isInternalLink(link)) {
            return;
        }
        emit linkAdded(toVariant(link));
    });
    connect(graph, &AudioGraph::linkRemoved, this, [this](const AudioLink &link) {
        if (isInternalLink(link)) {
            return;
        }
        emit linkRemoved(toVariant(link));
    });

    connect(volumeController, &VolumeController::volumeChanged, this,
            [this](uint32_t nodeId, float volume) {
                m_volumes.insert(nodeId, volume);
                QVariantMap payload;
                payload[QStringLiteral("nodeId")] = nodeId;
                payload[QStringLiteral("volume")] = volume;
                emit volumeChanged(payload);
            });
    connect(volumeController, &VolumeController::muteChanged, this,
            [this](uint32_t nodeId, bool muted) {
                m_muted.insert(nodeId, muted);
                QVariantMap payload;
                payload[QStringLiteral("nodeId")] = nodeId;
                payload[QStringLiteral("muted")] = muted;
                emit volumeChanged(payload);
            });
}

QVariantMap GraphBridge::toVariant(const AudioNode &node) const
{
    QVariantMap m;
    m[QStringLiteral("id")] = node.id;
    m[QStringLiteral("role")] = roleString(node.role);
    m[QStringLiteral("name")] = node.name;
    m[QStringLiteral("description")] = node.description;
    m[QStringLiteral("appName")] = node.appName;
    m[QStringLiteral("isVirtual")] = node.isVirtual;
    m[QStringLiteral("volume")] = m_volumes.value(node.id, 1.0);
    m[QStringLiteral("muted")] = m_muted.value(node.id, false);
    return m;
}

QVariantMap GraphBridge::toVariant(const AudioLink &link) const
{
    QVariantMap m;
    m[QStringLiteral("id")] = link.id;
    m[QStringLiteral("outputNodeId")] = link.outputNodeId;
    m[QStringLiteral("inputNodeId")] = link.inputNodeId;
    return m;
}

QVariantList GraphBridge::getNodes() const
{
    QVariantList list;
    for (const AudioNode &node : m_graph->nodes()) {
        if (node.isInternal) {
            continue;
        }
        if (node.isVirtual || m_autoReconnect->isPinned(identityForNode(node))) {
            list.append(toVariant(node));
        }
    }
    return list;
}

QVariantList GraphBridge::getLinks() const
{
    QVariantList list;
    for (const AudioLink &link : m_graph->links()) {
        if (isInternalLink(link)) {
            continue;
        }
        list.append(toVariant(link));
    }
    return list;
}

QVariantList GraphBridge::getAvailableNodes(const QString &side) const
{
    QVariantList list;
    for (const AudioNode &node : m_graph->nodes()) {
        if (node.isVirtual || node.isInternal) {
            continue; // virtuelle Geräte sind nie "hinzufügbar", nur löschbar; Internas gar nicht sichtbar
        }
        if (!roleMatchesSide(node.role, side)) {
            continue;
        }
        if (m_autoReconnect->isPinned(identityForNode(node))) {
            continue;
        }
        list.append(toVariant(node));
    }
    return list;
}

void GraphBridge::createLink(quint32 outputNodeId, quint32 inputNodeId)
{
    m_linkController->createLink(outputNodeId, inputNodeId);
}

void GraphBridge::removeLink(quint32 outputNodeId, quint32 inputNodeId)
{
    m_linkController->removeNodeLink(outputNodeId, inputNodeId);
}

void GraphBridge::setVolume(quint32 nodeId, double linearVolume)
{
    m_volumeController->setVolume(nodeId, static_cast<float>(linearVolume));
}

void GraphBridge::setMute(quint32 nodeId, bool muted)
{
    m_volumeController->setMute(nodeId, muted);
}

QString GraphBridge::createVirtualDevice(const QString &displayName)
{
    return m_virtualDevices->createVirtualDevice(displayName);
}

void GraphBridge::removeVirtualDevice(quint32 nodeId)
{
    const auto node = m_graph->node(nodeId);
    if (!node) {
        return;
    }
    const NodeIdentity identity = identityForNode(*node);
    if (identity.kind != QLatin1String("virtual")) {
        return;
    }
    m_virtualDevices->removeVirtualDevice(identity.key);
}

void GraphBridge::pinNode(quint32 nodeId)
{
    const auto node = m_graph->node(nodeId);
    if (!node || node->isVirtual) {
        return;
    }
    const NodeIdentity identity = identityForNode(*node);
    if (!identity.isValid() || m_autoReconnect->isPinned(identity)) {
        return;
    }
    m_autoReconnect->setPinned(identity, true);
    emit nodeAdded(toVariant(*node));
}

void GraphBridge::unpinNode(quint32 nodeId)
{
    const auto node = m_graph->node(nodeId);
    if (!node || node->isVirtual) {
        return;
    }
    const NodeIdentity identity = identityForNode(*node);
    if (!identity.isValid()) {
        return;
    }
    m_autoReconnect->setPinned(identity, false);
    emit nodeRemoved(nodeId);
}
