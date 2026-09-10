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

void LinkController::createLink(uint32_t outputNodeId, uint32_t inputNodeId)
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

    const int pairCount = std::min(outputPorts.size(), inputPorts.size());
    if (pairCount == 0) {
        return;
    }

    m_engine->runLocked([this, &outputPorts, &inputPorts, pairCount]() {
        for (int i = 0; i < pairCount; ++i) {
            const QByteArray outNodeId = QByteArray::number(outputPorts[i].nodeId);
            const QByteArray outPortId = QByteArray::number(outputPorts[i].id);
            const QByteArray inNodeId = QByteArray::number(inputPorts[i].nodeId);
            const QByteArray inPortId = QByteArray::number(inputPorts[i].id);

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

    emit linkRequested(outputNodeId, inputNodeId);
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
