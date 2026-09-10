#include "LinkController.h"

#include "AudioGraph.h"
#include "PipeWireEngine.h"

#include <QByteArray>

#include <algorithm>

#include <pipewire/pipewire.h>

LinkController::LinkController(PipeWireEngine *engine, AudioGraph *graph, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_graph(graph)
{
}

namespace {

// Port-IDs werden in Registrierungsreihenfolge vergeben, nicht in
// Kanalreihenfolge - ein naives Zippen nach Index kann z.B. FL mit FR
// verbinden, sobald zwei Nodes ihre Ports in unterschiedlicher Reihenfolge
// angemeldet haben (in der Praxis regelmäßig der Fall). Stattdessen zuerst
// nach audio.channel (FL/FR/...) paaren, danach übrig gebliebene Ports ohne
// eindeutiges Kanal-Match einfach der Reihe nach verbinden (deckt z.B.
// Mono-Geräte ab, die kein "MONO"-Label auf beiden Seiten führen).
QList<QPair<AudioPort, AudioPort>> pairPorts(QList<AudioPort> outputPorts,
                                              QList<AudioPort> inputPorts)
{
    QList<QPair<AudioPort, AudioPort>> pairs;

    for (int i = outputPorts.size() - 1; i >= 0; --i) {
        if (outputPorts[i].channel.isEmpty()) {
            continue;
        }
        const int j = std::find_if(inputPorts.begin(), inputPorts.end(),
                                    [&](const AudioPort &p) {
                                        return p.channel == outputPorts[i].channel;
                                    })
                      - inputPorts.begin();
        if (j < inputPorts.size()) {
            pairs.append({outputPorts[i], inputPorts[j]});
            outputPorts.removeAt(i);
            inputPorts.removeAt(j);
        }
    }

    const int leftover = std::min(outputPorts.size(), inputPorts.size());
    for (int i = 0; i < leftover; ++i) {
        pairs.append({outputPorts[i], inputPorts[i]});
    }

    return pairs;
}

} // namespace

void LinkController::createLink(uint32_t outputNodeId, uint32_t inputNodeId)
{
    if (doCreateLink(outputNodeId, inputNodeId)) {
        emit linkRequested(outputNodeId, inputNodeId);
    }
}

bool LinkController::createLinkSilent(uint32_t outputNodeId, uint32_t inputNodeId)
{
    return doCreateLink(outputNodeId, inputNodeId);
}

bool LinkController::doCreateLink(uint32_t outputNodeId, uint32_t inputNodeId)
{
    QList<AudioPort> outputPorts;
    for (const AudioPort &p : m_graph->portsForNode(outputNodeId)) {
        if (!p.isInput) {
            outputPorts.append(p);
        }
    }
    QList<AudioPort> inputPorts;
    for (const AudioPort &p : m_graph->portsForNode(inputNodeId)) {
        if (p.isInput) {
            inputPorts.append(p);
        }
    }

    const auto pairs = pairPorts(outputPorts, inputPorts);
    if (pairs.isEmpty()) {
        return false;
    }

    m_engine->runLocked([this, &pairs]() {
        for (const auto &pair : pairs) {
            const QByteArray outNodeId = QByteArray::number(pair.first.nodeId);
            const QByteArray outPortId = QByteArray::number(pair.first.id);
            const QByteArray inNodeId = QByteArray::number(pair.second.nodeId);
            const QByteArray inPortId = QByteArray::number(pair.second.id);

            struct pw_properties *props = pw_properties_new(
                PW_KEY_LINK_OUTPUT_NODE, outNodeId.constData(), PW_KEY_LINK_OUTPUT_PORT,
                outPortId.constData(), PW_KEY_LINK_INPUT_NODE, inNodeId.constData(),
                PW_KEY_LINK_INPUT_PORT, inPortId.constData(), nullptr);

            // Der zurückgegebene lokale Proxy wird bewusst nicht weiterverfolgt
            // (Erzeugung ist fire-and-forget; der eigentliche Link taucht über
            // den Registry-Listener im AudioGraph auf) und lebt bis zum
            // Prozessende.
            pw_core_create_object(m_engine->core(), "link-factory", PW_TYPE_INTERFACE_Link,
                                   PW_VERSION_LINK, &props->dict, 0);

            pw_properties_free(props);
        }
    });

    return true;
}

void LinkController::removeLink(uint32_t linkId)
{
    m_engine->runLocked(
        [this, linkId]() { pw_registry_destroy(m_engine->registry(), linkId); });
}

void LinkController::removeNodeLink(uint32_t outputNodeId, uint32_t inputNodeId)
{
    const auto links = m_graph->links();
    for (const AudioLink &link : links) {
        if (link.outputNodeId == outputNodeId && link.inputNodeId == inputNodeId) {
            removeLink(link.id);
        }
    }
}
