#include "AudioGraph.h"

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
    if (m_links.remove(id) > 0) {
        emit linkRemoved(id);
    }
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
