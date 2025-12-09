#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("logos-module-viewer");
    app.setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Logos Module Viewer - Inspect Qt plugin modules");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption moduleOption(QStringList() << "m" << "module",
                                   "Path to the module (.dylib/.so/.dll) to inspect",
                                   "path");
    parser.addOption(moduleOption);

    parser.process(app);

    QString modulePath;
    if (parser.isSet(moduleOption)) {
        modulePath = parser.value(moduleOption);
    }

    MainWindow window(modulePath);
    window.show();

    return app.exec();
}

