#include "MainWindow.h"

#include <QWebEngineView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_webView(new QWebEngineView(this))
{
    setWindowTitle(QStringLiteral("MixPipe"));
    resize(1440, 820);

    setCentralWidget(m_webView);
    m_webView->load(QUrl(QStringLiteral("qrc:/web/index.html")));
}
