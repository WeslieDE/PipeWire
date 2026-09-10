#pragma once

#include <QMainWindow>

class QWebEngineView;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QWebEngineView *m_webView;
};
