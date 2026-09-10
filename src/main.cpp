#include <QApplication>

#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MixPipe"));
    QApplication::setOrganizationName(QStringLiteral("tk.weslie"));

    MainWindow window;
    window.show();

    return QApplication::exec();
}
