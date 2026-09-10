#include "VolumeController.h"

#include "AudioGraph.h"
#include "PipeWireEngine.h"

#include <array>
#include <utility>

#include <pipewire/pipewire.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>

VolumeController::VolumeController(PipeWireEngine *engine, AudioGraph *graph, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
    connect(graph, &AudioGraph::nodeRemoved, this, &VolumeController::onNodeRemoved);
}

VolumeController::~VolumeController()
{
    m_engine->runLocked([this]() {
        for (auto *node : std::as_const(m_boundNodes)) {
            pw_proxy_destroy(reinterpret_cast<struct pw_proxy *>(node));
        }
    });
}

struct pw_node *VolumeController::boundNode(uint32_t nodeId)
{
    auto it = m_boundNodes.constFind(nodeId);
    if (it != m_boundNodes.constEnd()) {
        return *it;
    }

    auto *node = static_cast<struct pw_node *>(pw_registry_bind(
        m_engine->registry(), nodeId, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0));
    if (node) {
        m_boundNodes.insert(nodeId, node);
    }
    return node;
}

void VolumeController::onNodeRemoved(uint32_t nodeId)
{
    m_engine->runLocked([this, nodeId]() {
        auto it = m_boundNodes.find(nodeId);
        if (it == m_boundNodes.end()) {
            return;
        }
        pw_proxy_destroy(reinterpret_cast<struct pw_proxy *>(*it));
        m_boundNodes.erase(it);
    });
}

void VolumeController::setVolume(uint32_t nodeId, float linearVolume)
{
    m_engine->runLocked([this, nodeId, linearVolume]() {
        struct pw_node *node = boundNode(nodeId);
        if (!node) {
            return;
        }

        // SPA_PROP_softVolumes (nicht das rohe SPA_PROP_channelVolumes!) ist die
        // Property, die WirePlumber/wpctl/pavucontrol als "die" Lautstärke eines
        // Nodes behandeln - channelVolumes liegt darunter und wird von den
        // ALSA-/Stream-Plugins selbst verwaltet. Vereinfachung für die
        // MVP-Phase: Wir kennen die tatsächliche Kanalzahl des Nodes an dieser
        // Stelle nicht (dafür bräuchte es einen enum_params-Roundtrip vor dem
        // Setzen), daher wird ein Stereo-Array gesendet - der typische Fall für
        // Desktop-Audiogeräte/-Apps.
        std::array<float, 2> volumes{linearVolume, linearVolume};

        uint8_t buffer[512];
        struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        struct spa_pod *pod = static_cast<struct spa_pod *>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props, SPA_PROP_softVolumes,
            SPA_POD_Array(sizeof(float), SPA_TYPE_Float, volumes.size(), volumes.data())));

        pw_node_set_param(node, SPA_PARAM_Props, 0, pod);
    });

    emit volumeChanged(nodeId, linearVolume);
}

void VolumeController::setMute(uint32_t nodeId, bool muted)
{
    m_engine->runLocked([this, nodeId, muted]() {
        struct pw_node *node = boundNode(nodeId);
        if (!node) {
            return;
        }

        uint8_t buffer[512];
        struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        struct spa_pod *pod = static_cast<struct spa_pod *>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props, SPA_PROP_softMute,
            SPA_POD_Bool(muted)));

        pw_node_set_param(node, SPA_PARAM_Props, 0, pod);
    });
}
