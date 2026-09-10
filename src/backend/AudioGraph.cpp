#include "AudioGraph.h"

#include <QRegularExpression>
#include <QSet>

NodeIdentity identityForNode(const AudioNode &node)
{
    if (node.isVirtual) {
        // node.name ist "<handle>_sink" oder "<handle>_source" (siehe
        // VirtualDeviceManager) - beide Enden gehören zur selben Identität.
        QString handle = node.name;
        handle.remove(QRegularExpression(QStringLiteral("_(sink|source)$")));
        return {QStringLiteral("virtual"), handle};
    }
    if (!node.appName.isEmpty()) {
        const QString key = !node.processBinary.isEmpty() ? node.processBinary : node.appName;
        return {QStringLiteral("app"), key};
    }
    return {QStringLiteral("device"), node.name};
}

bool isSourceRole(NodeRole role)
{
    return role == NodeRole::SourceApp || role == NodeRole::SourceDevice;
}

bool isSinkRole(NodeRole role)
{
    return role == NodeRole::SinkDevice || role == NodeRole::SinkApp;
}

AudioGraph::AudioGraph(QObject *parent)
    : QObject(parent)
{
}

void AudioGraph::upsertNode(const AudioNode &node)
{
    const bool existed = m_nodes.contains(node.id);
    m_nodes.insert(node.id, node);
    if (existed) {
        emit nodeUpdated(node);
    } else {
        emit nodeAdded(node);
    }
}

void AudioGraph::removeNode(uint32_t id)
{
    if (m_nodes.remove(id) > 0) {
        emit nodeRemoved(id);
    }
}

void AudioGraph::upsertPort(const AudioPort &port)
{
    m_ports.insert(port.id, port);
    emit portAdded(port);
}

void AudioGraph::removePort(uint32_t id)
{
    m_ports.remove(id);
}

void AudioGraph::upsertLink(const AudioLink &link)
{
    const bool existed = m_links.contains(link.id);
    m_links.insert(link.id, link);
    if (!existed) {
        emit linkAdded(link);
    }
}

void AudioGraph::removeLink(uint32_t id)
{
    const auto it = m_links.constFind(id);
    if (it == m_links.constEnd()) {
        return;
    }
    const AudioLink removed = *it;
    m_links.remove(id);
    emit linkRemoved(removed);
}

QList<AudioNode> AudioGraph::nodes() const
{
    return m_nodes.values();
}

QList<AudioLink> AudioGraph::links() const
{
    return m_links.values();
}

QList<AudioPort> AudioGraph::portsForNode(uint32_t nodeId) const
{
    QList<AudioPort> result;
    for (const auto &port : m_ports) {
        if (port.nodeId == nodeId) {
            result.append(port);
        }
    }
    return result;
}

std::optional<AudioNode> AudioGraph::node(uint32_t id) const
{
    const auto it = m_nodes.constFind(id);
    if (it == m_nodes.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

std::optional<AudioPort> AudioGraph::port(uint32_t id) const
{
    const auto it = m_ports.constFind(id);
    if (it == m_ports.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

QList<QPair<uint32_t, uint32_t>> AudioGraph::nodeLinkPairs() const
{
    QSet<QPair<uint32_t, uint32_t>> unique;
    for (const auto &link : m_links) {
        unique.insert({link.outputNodeId, link.inputNodeId});
    }
    return unique.values();
}

std::optional<AudioNode> AudioGraph::findByIdentity(const NodeIdentity &identity) const
{
    if (!identity.isValid()) {
        return std::nullopt;
    }
    for (const auto &node : m_nodes) {
        if (identityForNode(node) == identity) {
            return node;
        }
    }
    return std::nullopt;
}
