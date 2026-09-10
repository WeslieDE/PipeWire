#pragma once

#include <QMap>
#include <QObject>
#include <cstdint>

struct pw_node;

class PipeWireEngine;
class AudioGraph;

// Setzt Lautstärke/Mute pro Node (SPA_PROP_channelVolumes/SPA_PROP_mute über
// pw_node_set_param). Bindet dafür pro Node einmalig einen pw_node-Proxy und
// hält ihn zwischengespeichert, statt bei jeder Slider-Bewegung neu zu binden.
class VolumeController : public QObject {
    Q_OBJECT

public:
    explicit VolumeController(PipeWireEngine *engine, AudioGraph *graph, QObject *parent = nullptr);
    ~VolumeController() override;

    // linearVolume: 0.0 (stumm) .. 1.0 (100%), wie in der UI dargestellt.
    void setVolume(uint32_t nodeId, float linearVolume);
    void setMute(uint32_t nodeId, bool muted);

signals:
    // Für AutoReconnectManager, um Lautstärken sitzungsübergreifend zu merken.
    void volumeChanged(uint32_t nodeId, float linearVolume);

private slots:
    void onNodeRemoved(uint32_t nodeId);

private:
    struct pw_node *boundNode(uint32_t nodeId);

    PipeWireEngine *m_engine;
    QMap<uint32_t, struct pw_node *> m_boundNodes;
};
