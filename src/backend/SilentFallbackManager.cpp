#include "SilentFallbackManager.h"

#include "AudioGraph.h"
#include "AutoReconnectManager.h"
#include "LinkController.h"
#include "VirtualDeviceManager.h"

#include <algorithm>

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
        // AutoReconnectManager::onPortAdded) - bei nodeAdded hat weder eine
        // frisch angeheftete App noch (insbesondere) unser eigenes
        // Fallback-Sink evtl. schon Ports, createLinkSilent() wäre dort ein
        // No-Op. Für Ports des Fallback-Sinks selbst daher alle wartenden
        // Apps erneut versuchen statt nur den (internen, für ensureFallbackLink
        // irrelevanten) Fallback-Node selbst.
        if (port.nodeId == m_fallbackSinkNodeId) {
            ensureFallbackForAllPinnedSourceApps();
            return;
        }
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

    // Nodes registrieren ihre Ports einzeln nacheinander (App-Seite UND -
    // besonders zu Beginn - unser eigenes Fallback-Sink), daher zählt ein
    // einzelner erfolgreicher LinkController-Aufruf nicht zwangsläufig schon
    // ALLE Kanäle (z.B. nur FL, weil playback_FR beim ersten Versuch noch
    // fehlte). AudioGraph::nodeLinkPairs() kennt nur "verbunden ja/nein" pro
    // Node-Paar, nicht die Kanalanzahl - hier stattdessen die tatsächliche
    // Link-Anzahl gegen die aktuell erwartbare Portanzahl vergleichen und bei
    // Unterdeckung erneut versuchen. LinkController::createLinkSilent()
    // berechnet die Portpaare bei jedem Aufruf neu und ist für bereits
    // bestehende Portpaare ein harmloser (vom PipeWire-Daemon mit "Datei
    // existiert bereits" quittierter) No-Op.
    int outputPortCount = 0;
    for (const AudioPort &p : m_graph->portsForNode(node.id)) {
        if (!p.isInput) {
            ++outputPortCount;
        }
    }
    int fallbackInputPortCount = 0;
    for (const AudioPort &p : m_graph->portsForNode(m_fallbackSinkNodeId)) {
        if (p.isInput) {
            ++fallbackInputPortCount;
        }
    }
    const int expectedLinks = std::min(outputPortCount, fallbackInputPortCount);
    if (expectedLinks == 0) {
        // Eine der beiden Seiten hat noch keine Ports - wird nachgeholt,
        // sobald der jeweilige portAdded-Trigger feuert.
        return;
    }

    int actualLinks = 0;
    for (const AudioLink &link : m_graph->links()) {
        if (link.outputNodeId == node.id && link.inputNodeId == m_fallbackSinkNodeId) {
            ++actualLinks;
        }
    }
    if (actualLinks >= expectedLinks) {
        return;
    }

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
