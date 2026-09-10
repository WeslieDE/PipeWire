#include "SilentFallbackManager.h"

#include "AudioGraph.h"
#include "AutoReconnectManager.h"
#include "LinkController.h"
#include "VirtualDeviceManager.h"

SilentFallbackManager::SilentFallbackManager(AudioGraph *graph, LinkController *linkController,
                                              VirtualDeviceManager *virtualDevices,
                                              AutoReconnectManager *autoReconnect, QObject *parent)
    : QObject(parent)
    , m_graph(graph)
    , m_linkController(linkController)
    , m_virtualDevices(virtualDevices)
    , m_autoReconnect(autoReconnect)
{
    connect(graph, &AudioGraph::nodeAdded, this, [this](const AudioNode &node) {
        if (node.isInternal && node.role == NodeRole::SinkDevice && m_fallbackSinkNodeId == 0) {
            // Das ist das capture-seitige ("_sink") Ende unseres eigenen
            // Fallback-Loopbacks - ab jetzt als Ausweich-Ziel verwendbar.
            m_fallbackSinkNodeId = node.id;
            ensureFallbackForAllPinnedSourceApps();
            return;
        }
        ensureFallbackLink(node);
    });
    connect(graph, &AudioGraph::portAdded, this, [this](const AudioPort &port) {
        // Ports registrieren sich asynchron nach ihrem Node (siehe
        // AutoReconnectManager::onPortAdded) - bei nodeAdded hat eine frisch
        // angeheftete App evtl. noch keine Ports, createLinkSilent() wäre
        // dort ein No-Op. Das erneute Vervollständigen, sobald z.B. auch das
        // eigene Fallback-Sink seine Ports bekommt, übernimmt
        // LinkController::healPair() (getriggert von dessen eigenem
        // portAdded-Listener) automatisch für jedes einmal angefragte Paar.
        const auto node = m_graph->node(port.nodeId);
        if (node) {
            ensureFallbackLink(*node);
        }
    });
    connect(autoReconnect, &AutoReconnectManager::nodePinned, this,
            [this](const NodeIdentity &identity) {
                const auto node = m_graph->findByIdentity(identity);
                if (node) {
                    ensureFallbackLink(*node);
                }
            });
}

void SilentFallbackManager::start()
{
    m_virtualDevices->ensureFallbackDevice();
}

void SilentFallbackManager::ensureFallbackLink(const AudioNode &node)
{
    if (node.role != NodeRole::SourceApp || node.isInternal) {
        return;
    }
    if (m_fallbackSinkNodeId == 0) {
        // Fallback-Gerät noch nicht bereit - wird nachgeholt, sobald dessen
        // Node auftaucht (siehe ensureFallbackForAllPinnedSourceApps oben).
        return;
    }
    const NodeIdentity identity = identityForNode(node);
    if (!m_autoReconnect->isPinned(identity)) {
        // Nur für in MixPipe sichtbare/verwaltete Apps - unangeheftete Apps
        // überlässt MixPipe komplett WirePlumbers eigenem Default-Routing.
        return;
    }

    // LinkController::createLinkSilent() registriert das Paar als "soll
    // verbunden bleiben" und vervollständigt es selbstständig, sobald weitere
    // Ports einer der beiden Seiten auftauchen (siehe
    // LinkController::healPair) - wiederholte Aufrufe hier sind idempotent
    // (kein erneuter Verbindungsversuch, sobald die erwartete Kanalzahl schon
    // erreicht ist).
    m_linkController->createLinkSilent(node.id, m_fallbackSinkNodeId);
}

void SilentFallbackManager::ensureFallbackForAllPinnedSourceApps()
{
    for (const AudioNode &node : m_graph->nodes()) {
        ensureFallbackLink(node);
    }
}

void SilentFallbackManager::beforeShutdown(const QString &defaultSinkName)
{
    if (defaultSinkName.isEmpty() || m_fallbackSinkNodeId == 0) {
        return;
    }
    const auto defaultSink = m_graph->findByIdentity({QStringLiteral("device"), defaultSinkName});
    if (!defaultSink) {
        return;
    }

    const auto pairs = m_graph->nodeLinkPairs();
    for (const auto &pair : pairs) {
        if (pair.second != m_fallbackSinkNodeId) {
            continue;
        }
        const uint32_t appNodeId = pair.first;

        // Nur relinken, wenn die (gleich verschwindende) Fallback-Verbindung
        // die einzige ist - hat die App bereits eine echte Verbindung, bleibt
        // die davon unberührt bestehen.
        bool hasRealLink = false;
        for (const auto &other : pairs) {
            if (other.first == appNodeId && other.second != m_fallbackSinkNodeId) {
                hasRealLink = true;
                break;
            }
        }
        if (hasRealLink) {
            continue;
        }

        m_linkController->createLinkSilent(appNodeId, defaultSink->id);
    }
}
