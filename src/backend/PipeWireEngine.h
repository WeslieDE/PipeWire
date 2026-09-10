#pragma once

#include <QObject>
#include <QString>

#include <functional>

#include <pipewire/pipewire.h>

class AudioGraph;

// Verbindet sich als eigenständiger PipeWire-Client mit dem laufenden System-Daemon,
// beobachtet den Graphen per Registry-Listener und spiegelt erkannte Nodes/Ports/Links
// klassifiziert in ein AudioGraph-Modell. Läuft komplett auf einem eigenen
// pw_thread_loop-Thread; alle Zugriffe auf AudioGraph erfolgen über Qt-Signale, die
// automatisch auf den Thread des Empfängers (Qt-Main-Thread) marshalled werden.
class PipeWireEngine : public QObject {
    Q_OBJECT

public:
    explicit PipeWireEngine(AudioGraph *graph, QObject *parent = nullptr);
    ~PipeWireEngine() override;

    PipeWireEngine(const PipeWireEngine &) = delete;
    PipeWireEngine &operator=(const PipeWireEngine &) = delete;

    // Startet den PipeWire-Thread und verbindet zum Daemon. false bei Fehler
    // (z.B. kein laufender PipeWire-Dienst erreichbar).
    bool start();
    void stop();

    struct pw_core *core() const { return m_core; }
    struct pw_context *context() const { return m_context; }
    struct pw_registry *registry() const { return m_registry; }

    // Führt fn() mit gehaltenem pw_thread_loop-Lock aus. Muss für jeden Aufruf
    // einer PipeWire-Funktion verwendet werden, der von außerhalb des
    // PipeWire-Threads kommt (z.B. Link-/Volume-Änderungen vom Qt-Main-Thread).
    void runLocked(const std::function<void()> &fn);

private:
    static void onGlobalAdded(void *data, uint32_t id, uint32_t permissions,
                               const char *type, uint32_t version,
                               const struct spa_dict *props);
    static void onGlobalRemoved(void *data, uint32_t id);

    void handleNodeGlobal(uint32_t id, const struct spa_dict *props);
    void handlePortGlobal(uint32_t id, const struct spa_dict *props);
    void handleLinkGlobal(uint32_t id, const struct spa_dict *props);

    AudioGraph *m_graph;

    struct pw_thread_loop *m_loop = nullptr;
    struct pw_context *m_context = nullptr;
    struct pw_core *m_core = nullptr;
    struct pw_registry *m_registry = nullptr;
    struct spa_hook m_registryListener {};

    bool m_running = false;
};
