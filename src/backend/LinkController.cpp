#include "LinkController.h"

#include "AudioGraph.h"
#include "PipeWireEngine.h"

#include <QByteArray>

#include <pipewire/pipewire.h>

LinkController::LinkController(PipeWireEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

void LinkController::createLink(uint32_t outputNodeId, uint32_t inputNodeId)
{
    m_engine->runLocked([this, outputNodeId, inputNodeId]() {
        const QByteArray outId = QByteArray::number(outputNodeId);
        const QByteArray inId = QByteArray::number(inputNodeId);

        struct pw_properties *props = pw_properties_new(
            PW_KEY_LINK_OUTPUT_NODE, outId.constData(), PW_KEY_LINK_INPUT_NODE,
            inId.constData(), nullptr);

        // Absichtlich keine Port-IDs: PipeWires Link-Factory verhandelt die
        // passenden Ports (z.B. beide Stereo-Kanäle) selbst. Der zurückgegebene
        // lokale Proxy wird bewusst nicht weiterverfolgt (Erzeugung ist
        // fire-and-forget; der eigentliche Link taucht über den
        // Registry-Listener im AudioGraph auf) und lebt bis zum Prozessende.
        pw_core_create_object(m_engine->core(), "link-factory", PW_TYPE_INTERFACE_Link,
                               PW_VERSION_LINK, &props->dict, 0);

        pw_properties_free(props);
    });
}

void LinkController::removeLink(uint32_t linkId)
{
    m_engine->runLocked(
        [this, linkId]() { pw_registry_destroy(m_engine->registry(), linkId); });
}

void LinkController::removeNodeLink(AudioGraph *graph, uint32_t outputNodeId, uint32_t inputNodeId)
{
    const auto links = graph->links();
    for (const AudioLink &link : links) {
        if (link.outputNodeId == outputNodeId && link.inputNodeId == inputNodeId) {
            removeLink(link.id);
        }
    }
}
