#include "mainwindow.h"
#include "viewer_core.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

#include <cstdio>

#ifdef ENABLE_QML_INSPECTOR
#include "inspectorserver.h"
#endif

// logos-module-viewer is a GUI inspector for Logos modules. With a window it
// lists a module's methods and lets you call them and watch events; with the
// headless flags below it does the same non-interactively so it can be scripted.
//
//   --module/-m <path>     module plugin to inspect
//   --list-methods         print the module's methods (and metadata) and exit
//   --call "m(a, b)"       load + call a method over IPC, print the result; repeatable
//   --json                 machine-readable output for the modes above
//
// When no headless flag is given it launches the GUI window. If built with
// ENABLE_QML_INSPECTOR, the GUI also embeds a QObject-tree inspector
// (logos-qt-mcp) so headless ui_test doc-tests can drive it and screenshot it.

namespace {

QJsonObject methodToJson(const ModuleLib::MethodInfo& m)
{
    QJsonObject obj;
    obj["name"] = m.name;
    obj["signature"] = m.signature;
    obj["returnType"] = m.returnType;
    if (!m.description.isEmpty()) {
        obj["description"] = m.description;
    }
    QJsonArray params;
    for (const ModuleLib::ParameterInfo& p : m.parameters) {
        QJsonObject po;
        po["name"] = p.name;
        po["type"] = p.type;
        params.append(po);
    }
    obj["parameters"] = params;
    return obj;
}

// Render the loaded module's methods either as JSON or as a human-readable list.
int runListMethods(ViewerCore& core, const ViewerCore::ModuleInfo& info, bool asJson)
{
    QTextStream out(stdout);

    if (asJson) {
        QJsonObject root;
        root["name"] = info.name;
        root["version"] = info.version;
        root["type"] = info.type;
        root["path"] = info.path;
        QJsonArray methods;
        for (const ModuleLib::MethodInfo& m : info.methods) {
            methods.append(methodToJson(m));
        }
        root["methods"] = methods;
        QJsonArray events;
        for (const ModuleLib::MethodInfo& e : info.events) {
            events.append(methodToJson(e));
        }
        root["events"] = events;
        out << QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
        return 0;
    }

    out << "Module:  " << info.name;
    if (!info.version.isEmpty()) out << " v" << info.version;
    out << "\n";
    if (!info.type.isEmpty()) out << "Type:    " << info.type << "\n";
    out << "Path:    " << info.path << "\n\n";

    out << "Methods:\n";
    if (info.methods.empty()) {
        out << "  (none)\n";
    }
    for (const ModuleLib::MethodInfo& m : info.methods) {
        const QString rt = m.returnType.isEmpty() ? QStringLiteral("void") : m.returnType;
        QStringList ps;
        for (const ModuleLib::ParameterInfo& p : m.parameters) {
            ps << QStringLiteral("%1 %2").arg(p.type, p.name);
        }
        out << "  " << rt << " " << m.name << "(" << ps.join(QStringLiteral(", ")) << ")\n";
    }

    if (!info.events.empty()) {
        out << "\nEvents:\n";
        for (const ModuleLib::MethodInfo& e : info.events) {
            out << "  " << e.name << "\n";
        }
    }
    Q_UNUSED(core);
    return 0;
}

// Execute each --call expression sequentially; print a JSON result per call.
int runCalls(ViewerCore& core, const QStringList& calls, bool asJson)
{
    QTextStream out(stdout);
    QTextStream err(stderr);
    int exitCode = 0;

    for (const QString& expr : calls) {
        const ViewerCore::CallResult r = core.callExpression(expr);

        if (!r.ok) {
            err << "Error calling " << expr << ": " << r.error << "\n";
            exitCode = 1;
            if (asJson) {
                QJsonObject o;
                o["call"] = expr;
                o["error"] = r.error;
                out << QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)) << "\n";
            }
            continue;
        }

        // Mirror logoscore's output shape: {"result": <value>} (or void).
        QJsonObject o;
        o["call"] = expr;
        if (r.isVoid) {
            o["result"] = QJsonValue::Null;
            o["void"] = true;
        } else {
            o["result"] = QJsonValue::fromVariant(r.value);
        }

        if (asJson) {
            out << QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)) << "\n";
        } else {
            if (r.isVoid) {
                out << expr << " => (void)\n";
            } else {
                out << expr << " => "
                    << QString::fromUtf8(QJsonDocument(QJsonObject{{"result", o["result"]}})
                                             .toJson(QJsonDocument::Compact))
                    << "\n";
            }
        }
    }
    return exitCode;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("logos-module-viewer"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Logos Module Viewer - inspect and exercise Logos module plugins"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption moduleOption(
        QStringList() << "m" << "module",
        QStringLiteral("Path to the module plugin (.so/.dylib/.dll) to inspect"),
        QStringLiteral("path"));
    parser.addOption(moduleOption);

    QCommandLineOption listOption(
        QStringLiteral("list-methods"),
        QStringLiteral("Print the module's methods and exit (headless)"));
    parser.addOption(listOption);

    QCommandLineOption callOption(
        QStringLiteral("call"),
        QStringLiteral("Call a method, e.g. \"method(arg1, arg2)\" (repeatable, headless)"),
        QStringLiteral("expr"));
    parser.addOption(callOption);

    QCommandLineOption modulesDirOption(
        QStringLiteral("modules-dir"),
        QStringLiteral("Directory liblogos scans for modules and dependencies"),
        QStringLiteral("dir"));
    parser.addOption(modulesDirOption);

    QCommandLineOption preloadOption(
        QStringLiteral("preload"),
        QStringLiteral("Comma-separated modules to load before the target (e.g. capability_module)"),
        QStringLiteral("mods"));
    parser.addOption(preloadOption);

    QCommandLineOption jsonOption(
        QStringLiteral("json"),
        QStringLiteral("Machine-readable JSON output for headless modes"));
    parser.addOption(jsonOption);

    parser.process(app);

    const QString modulePath = parser.value(moduleOption);
    const bool listMethods = parser.isSet(listOption);
    const QStringList calls = parser.values(callOption);
    const bool asJson = parser.isSet(jsonOption);
    const bool headless = listMethods || !calls.isEmpty();

    if (headless) {
        if (modulePath.isEmpty()) {
            QTextStream(stderr) << "Error: --module/-m is required for headless modes\n";
            return 2;
        }

        ViewerCore core;
        if (parser.isSet(modulesDirOption)) {
            core.setModulesDir(parser.value(modulesDirOption));
        }
        if (parser.isSet(preloadOption)) {
            core.setPreloadModules(parser.value(preloadOption)
                                       .split(QLatin1Char(','), Qt::SkipEmptyParts));
        }

        const ViewerCore::ModuleInfo info = core.loadModule(modulePath);
        if (!info.ok) {
            QTextStream(stderr) << "Error: " << info.error << "\n";
            return 1;
        }

        int rc = 0;
        if (listMethods) {
            rc = runListMethods(core, info, asJson);
        }
        if (rc == 0 && !calls.isEmpty()) {
            rc = runCalls(core, calls, asJson);
        }
        return rc;
    }

    // Interactive GUI mode. The module path and modules dir may also come from the
    // environment so launchers (e.g. the ui_test doc-test) can load a module
    // without threading command-line args through `nix run`.
    QString guiModulePath = modulePath;
    if (guiModulePath.isEmpty()) {
        guiModulePath = qEnvironmentVariable("LOGOS_MODULE_VIEWER_MODULE");
    }
    QString guiModulesDir = parser.value(modulesDirOption);
    if (guiModulesDir.isEmpty()) {
        guiModulesDir = qEnvironmentVariable("LOGOS_MODULE_VIEWER_MODULES_DIR");
    }

    MainWindow window(guiModulePath, guiModulesDir);
    window.show();

#ifdef ENABLE_QML_INSPECTOR
    // Embed the QObject-tree inspector so headless ui_test doc-tests can drive
    // the window and capture screenshots (listens on QML_INSPECTOR_PORT, def 3768).
    InspectorServer::attach(&window);
#endif

    return app.exec();
}
