#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("CppVisualIDE"));
    QApplication::setApplicationName(QStringLiteral("C++ Visual IDE"));

    MainWindow window;
    window.show();
    return app.exec();
}
