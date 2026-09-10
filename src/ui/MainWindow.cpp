#include "MainWindow.h"

#include "backend/AudioGraph.h"
#include "backend/AutoReconnectManager.h"
#include "backend/LinkController.h"
#include "backend/PipeWireEngine.h"
#include "backend/VirtualDeviceManager.h"
#include "backend/VolumeController.h"
#include "bridge/GraphBridge.h"

#include <QColor>
#include <QWebChannel>
#include <QWebEngineView>

#include <cstdio>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_webView(new QWebEngineView(this))
    , m_webChannel(new QWebChannel(this))
    , m_graph(new AudioGraph(this))
    , m_engine(new PipeWireEngine(m_graph, this))
    , m_linkController(new LinkController(m_engine, m_graph, this))
    , m_volumeController(new VolumeController(m_engine, m_graph, this))
    , m_virtualDevices(new VirtualDeviceManager(m_engine, this))
    , m_bridge(new GraphBridge(m_graph, m_linkController, m_volumeController, m_virtualDevices,
                                this))
    , m_autoReconnect(new AutoReconnectManager(m_graph, m_linkController, m_volumeController,
                                                m_virtualDevices, this))
{
    setWindowTitle(QStringLiteral("MixPipe"));
    resize(1440, 820);

    // QWebEngineView paints white by default until the page has loaded and
    // painted its own (dark) body background - noticeable here since
    // PipeWireEngine::start() below runs before load() even begins. Match
    // web/tokens.css' --color-paper so there is no white flash.
    m_webView->page()->setBackgroundColor(QColor(0x0f, 0x10, 0x14));

    if (!m_engine->start()) {
        fprintf(stderr, "MainWindow: PipeWireEngine::start() fehlgeschlagen - läuft kein "
                         "PipeWire-Dienst?\n");
    } else {
        m_autoReconnect->restoreSession();
    }

    m_webChannel->registerObject(QStringLiteral("graphBridge"), m_bridge);
    m_webView->page()->setWebChannel(m_webChannel);

    setCentralWidget(m_webView);
    m_webView->load(QUrl(QStringLiteral("qrc:/web/index.html")));
}

MainWindow::~MainWindow() = default;
