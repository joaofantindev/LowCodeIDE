#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/images/mascot.png")));
    QApplication::setOrganizationName(QStringLiteral("CppVisualIDE"));
    QApplication::setApplicationName(QStringLiteral("C++ Visual IDE"));

    MainWindow window;
    window.show();
    if (argc > 1) {
        window.openProjectPath(QString::fromLocal8Bit(argv[1]));
    }
    return app.exec();
}
