#include "AutoReconnectManager.h"

#include "AudioGraph.h"
#include "LinkController.h"
#include "VirtualDeviceManager.h"
#include "VolumeController.h"

namespace {
constexpr int kSaveDebounceMs = 500;
}

AutoReconnectManager::AutoReconnectManager(AudioGraph *graph, LinkController *linkController,
                                            VolumeController *volumeController,
                                            VirtualDeviceManager *virtualDevices, QObject *parent)
    : QObject(parent)
    , m_graph(graph)
    , m_linkController(linkController)
    , m_volumeController(volumeController)
    , m_virtualDevices(virtualDevices)
{
    m_saveTimer.setSingleShot(true);
    connect(&m_saveTimer, &QTimer::timeout, this, &AutoReconnectManager::saveNow);

    connect(graph, &AudioGraph::nodeAdded, this, &AutoReconnectManager::onNodeAdded);
    connect(graph, &AudioGraph::portAdded, this, &AutoReconnectManager::onPortAdded);
    connect(graph, &AudioGraph::nodeRemoved, this, &AutoReconnectManager::onNodeRemoved);
    connect(graph, &AudioGraph::linkRemoved, this, &AutoReconnectManager::onLinkRemoved);
    connect(linkController, &LinkController::linkRequested, this,
            &AutoReconnectManager::onLinkRequested);

    connect(virtualDevices, &VirtualDeviceManager::deviceCreated, this,
            [this](const QString &handleId, const QString &displayName) {
                m_virtualDeviceEntries.append({handleId, displayName});
                scheduleSave();
            });
    connect(virtualDevices, &VirtualDeviceManager::deviceRemoved, this,
            [this](const QString &handleId) {
                for (int i = 0; i < m_virtualDeviceEntries.size(); ++i) {
                    if (m_virtualDeviceEntries[i].first == handleId) {
                        m_virtualDeviceEntries.removeAt(i);
                        break;
                    }
                }
                scheduleSave();
            });

    connect(volumeController, &VolumeController::volumeChanged, this,
            [this](uint32_t nodeId, float volume) {
                const auto node = m_graph->node(nodeId);
                if (!node) {
                    return;
                }
                const NodeIdentity identity = identityForNode(*node);
                if (!identity.isValid()) {
                    return;
                }
                bool replaced = false;
                for (auto &nv : m_nodeVolumes) {
                    if (nv.kind == identity.kind && nv.key == identity.key) {
                        nv.volume = volume;
                        replaced = true;
                        break;
                    }
                }
                if (!replaced) {
                    m_nodeVolumes.append({identity.kind, identity.key, volume});
                }
                scheduleSave();
            });
}

void AutoReconnectManager::restoreSession()
{
    const SessionStore::Session session = m_store.load();
    m_linkRules = session.linkRules;
    m_nodeVolumes = session.nodeVolumes;

    // Reihenfolge ist wichtig: VirtualDeviceManagers interner Zähler beginnt
    // pro Sitzung wieder bei 1, dadurch entstehen bei gleicher
    // Erstellungsreihenfolge wieder dieselben Handle-IDs wie beim letzten Mal
    // (z.B. "mixpipe_virtual_1"), auf die sich gespeicherte Regeln beziehen.
    for (const QString &displayName : session.virtualDevices) {
        m_virtualDevices->createVirtualDevice(displayName);
    }
}

void AutoReconnectManager::onNodeAdded(const AudioNode &node)
{
    const NodeIdentity identity = identityForNode(node);
    if (identity.isValid()) {
        for (const auto &nv : m_nodeVolumes) {
            if (nv.kind == identity.kind && nv.key == identity.key) {
                m_volumeController->setVolume(node.id, nv.volume);
                break;
            }
        }
    }
    applyRulesFor(node);
}

void AutoReconnectManager::onPortAdded(const AudioPort &port)
{
    // Ports registrieren sich asynchron NACH ihrem Node; ein Node kann daher
    // beim nodeAdded-Zeitpunkt noch keine Ports haben, weshalb createLink()
    // dort still nichts tut. Bei jedem neuen Port erneut versuchen zu
    // verlinken - idempotent, da applyRulesFor/createLink bestehende Links
    // überspringen.
    const auto node = m_graph->node(port.nodeId);
    if (node) {
        applyRulesFor(*node);
    }
}

void AutoReconnectManager::onNodeRemoved(uint32_t /*id*/)
{
    scheduleSave();
}

void AutoReconnectManager::onLinkRequested(uint32_t outputNodeId, uint32_t inputNodeId)
{
    m_managedPairs.insert({outputNodeId, inputNodeId});
    scheduleSave();
}

void AutoReconnectManager::onLinkRemoved(const AudioLink &link)
{
    const QPair<uint32_t, uint32_t> pair{link.outputNodeId, link.inputNodeId};
    if (!m_graph->nodeLinkPairs().contains(pair)) {
        m_managedPairs.remove(pair);
    }
    scheduleSave();
}

void AutoReconnectManager::applyRulesFor(const AudioNode &node)
{
    const NodeIdentity nodeIdentity = identityForNode(node);
    if (!nodeIdentity.isValid()) {
        return;
    }

    for (const auto &rule : m_linkRules) {
        const NodeIdentity sourceIdentity{rule.sourceKind, rule.sourceKey};
        const NodeIdentity targetIdentity{rule.targetKind, rule.targetKey};

        uint32_t sourceNodeId = 0;
        uint32_t targetNodeId = 0;

        if (nodeIdentity == sourceIdentity) {
            const auto target = m_graph->findByIdentity(targetIdentity);
            if (!target) {
                continue;
            }
            sourceNodeId = node.id;
            targetNodeId = target->id;
        } else if (nodeIdentity == targetIdentity) {
            const auto source = m_graph->findByIdentity(sourceIdentity);
            if (!source) {
                continue;
            }
            sourceNodeId = source->id;
            targetNodeId = node.id;
        } else {
            continue;
        }

        const QPair<uint32_t, uint32_t> pair{sourceNodeId, targetNodeId};
        if (m_graph->nodeLinkPairs().contains(pair) || m_managedPairs.contains(pair)) {
            // Schon verbunden oder Anfrage bereits unterwegs (Ports eines
            // Nodes registrieren sich einzeln nacheinander, applyRulesFor
            // läuft daher mehrfach kurz hintereinander für denselben Node).
            continue;
        }

        m_linkController->createLink(sourceNodeId, targetNodeId);
    }
}

void AutoReconnectManager::scheduleSave()
{
    m_saveTimer.start(kSaveDebounceMs);
}

void AutoReconnectManager::saveNow()
{
    SessionStore::Session session;
    for (const auto &entry : m_virtualDeviceEntries) {
        session.virtualDevices.append(entry.second);
    }
    session.nodeVolumes = m_nodeVolumes;

    const auto liveLinks = m_graph->nodeLinkPairs();
    for (const auto &pair : m_managedPairs) {
        if (!liveLinks.contains(pair)) {
            continue;
        }
        const auto outputNode = m_graph->node(pair.first);
        const auto inputNode = m_graph->node(pair.second);
        if (!outputNode || !inputNode) {
            continue;
        }
        const NodeIdentity sourceIdentity = identityForNode(*outputNode);
        const NodeIdentity targetIdentity = identityForNode(*inputNode);
        if (!sourceIdentity.isValid() || !targetIdentity.isValid()) {
            continue;
        }
        session.linkRules.append({sourceIdentity.kind, sourceIdentity.key, targetIdentity.kind,
                                   targetIdentity.key});
    }

    m_store.save(session);
}
