#pragma once

#include <QMainWindow>

class QWebEngineView;
class QWebChannel;
class AudioGraph;
class PipeWireEngine;
class LinkController;
class VolumeController;
class VirtualDeviceManager;
class AutoReconnectManager;
class GraphBridge;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    QWebEngineView *m_webView;
    QWebChannel *m_webChannel;

    // Konstruktions-/Zerstörungsreihenfolge ist bewusst so gewählt (siehe
    // .cpp): PipeWireEngine muss alle Controller, die m_engine->runLocked()
    // in ihrem Destruktor nutzen, überleben. Qt zerstört QObject-Kinder in
    // umgekehrter Erzeugungsreihenfolge, was genau das sicherstellt.
    AudioGraph *m_graph;
    PipeWireEngine *m_engine;
    LinkController *m_linkController;
    VolumeController *m_volumeController;
    VirtualDeviceManager *m_virtualDevices;
    AutoReconnectManager *m_autoReconnect;
    GraphBridge *m_bridge;
};
