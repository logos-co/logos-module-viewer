#ifndef VIEWER_CORE_H
#define VIEWER_CORE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <functional>
#include <vector>

#include "logos_module.h"   // ModuleLib::LogosModule, MethodInfo, ModuleMetadata

class LogosAPI;
class LogosAPIClient;
class LogosObject;

// ViewerCore holds all of logos-module-viewer's non-GUI logic so the same code
// path backs both the Qt Widgets window (mainwindow.cpp) and the headless
// --list-methods / --call modes (main.cpp).
//
// Responsibilities:
//   * own the liblogos runtime lifecycle (init/add-modules-dir/start/cleanup);
//   * introspect a module plugin via ModuleLib::LogosModule (works for both
//     legacy Q_INVOKABLE plugins and modern universal/cdylib provider plugins);
//   * load the module into the running core and call its methods over the
//     Logos IPC bridge (LogosAPI -> LogosAPIClient::invokeRemoteMethod);
//   * subscribe to module events.
//
// A single ViewerCore instance manages one loaded module at a time. Construct
// it once, call loadModule() with a plugin path, then introspect / call / watch.
class ViewerCore : public QObject
{
    Q_OBJECT

public:
    // Outcome of introspecting a module file. methods/events come from
    // ModuleLib::LogosModule, so the universal-vs-legacy distinction is handled
    // transparently for us.
    struct ModuleInfo {
        bool ok = false;
        QString error;
        QString name;        // remote module name (e.g. "accounts_module")
        QString version;
        QString description;
        QString type;
        QString path;        // resolved absolute path to the plugin
        std::vector<ModuleLib::MethodInfo> methods;
        std::vector<ModuleLib::MethodInfo> events;
    };

    // Result of a remote method call.
    struct CallResult {
        bool ok = false;
        QString error;
        QVariant value;       // raw return value (invalid for void)
        bool isVoid = false;  // method has void / empty return type
    };

    // Callback delivered for every received event: (eventName, eventData).
    using EventHandler = std::function<void(const QString&, const QVariantList&)>;

    explicit ViewerCore(QObject* parent = nullptr);
    ~ViewerCore() override;

    // Directory liblogos scans for modules (and their dependencies). Defaults to
    // "<app-dir>/../modules". Must be set before the first loadModule() that
    // needs the core (i.e. before any call/subscribe). No-op once the core has
    // started.
    void setModulesDir(const QString& dir);
    QString modulesDir() const { return m_modulesDir; }

    // Modules to auto-load into the core before the target module (e.g.
    // "capability_module" for the inter-module auth handshake). Loaded in order
    // the first time the core starts.
    void setPreloadModules(const QStringList& modules) { m_preloadModules = modules; }

    // Introspect a plugin file WITHOUT starting the core or loading it for IPC.
    // Safe to call in a purely offline context (e.g. --list-methods). Also
    // records the module as the current target for subsequent call/subscribe.
    ModuleInfo loadModule(const QString& path);

    // The currently targeted module (last successful loadModule()).
    const ModuleInfo& currentModule() const { return m_module; }
    bool hasModule() const { return m_module.ok; }

    // Ensure the liblogos core is running and the current module is loaded and
    // its IPC client is connected. Idempotent. Returns false (with *error set)
    // on failure. Called automatically by callMethod()/subscribeEvent().
    bool ensureLoaded(QString* error = nullptr);

    // Invoke a method on the current module over IPC. Coerces string arguments
    // to the parameter types declared by the method's signature.
    CallResult callMethod(const QString& methodName, const QVariantList& args);

    // Parse a "method(arg1, arg2)" string (the logoscore -c syntax) and call it.
    // Arguments are split on commas and coerced per the method signature;
    // @filename loads file contents as the argument.
    CallResult callExpression(const QString& expression);

    // Subscribe to a named event on the current module. The handler fires on the
    // Qt event loop for each received event. Returns false on failure.
    bool subscribeEvent(const QString& eventName, EventHandler handler, QString* error = nullptr);

    // Coerce a single textual value to a QVariant of the given C++/Qt type name
    // (e.g. "int", "double", "bool", "QString"). Shared by the GUI forms and the
    // --call parser.
    static QVariant coerceArg(const QString& typeName, const QString& text);

    // True if a return type name denotes void / no return.
    static bool isVoidType(const QString& returnType);

private:
    bool startCore(QString* error);

    QString m_modulesDir;
    QStringList m_preloadModules;
    bool m_coreInitialized = false;

    ModuleInfo m_module;                 // current target module
    ModuleLib::LogosModule m_loaded;     // RAII handle (introspection only)
    bool m_moduleLoadedInCore = false;   // processed + loaded into liblogos

    LogosAPI* m_logosAPI = nullptr;
    std::vector<LogosObject*> m_eventObjects;  // kept alive for subscriptions
};

#endif // VIEWER_CORE_H
