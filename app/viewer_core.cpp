#include "viewer_core.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QElapsedTimer>

#include "logos_api.h"
#include "logos_api_client.h"

// liblogos C runtime. Symbols live in liblogos_core (which also carries the
// aggregated LogosAPI/LogosAPIClient symbols post Qt-split).
extern "C" {
    void  logos_core_init(int argc, char* argv[]);
    void  logos_core_add_modules_dir(const char* modules_dir);
    void  logos_core_start();
    void  logos_core_cleanup();
    char* logos_core_process_module(const char* module_path);
    int   logos_core_load_module(const char* module_name, bool with_dependencies);
}

namespace {

// Strip const/&/* and whitespace so "const QString&" -> "QString".
QString normalizeType(const QString& type)
{
    QString t = type;
    t.remove(QStringLiteral("const "));
    t.remove(QLatin1Char('&'));
    t.remove(QLatin1Char('*'));
    return t.trimmed();
}

} // namespace

ViewerCore::ViewerCore(QObject* parent)
    : QObject(parent)
{
}

ViewerCore::~ViewerCore()
{
    if (m_logosAPI) {
        delete m_logosAPI;
        m_logosAPI = nullptr;
    }
    if (m_coreInitialized) {
        logos_core_cleanup();
        m_coreInitialized = false;
    }
}

void ViewerCore::setModulesDir(const QString& dir)
{
    m_modulesDir = dir;
}

bool ViewerCore::isVoidType(const QString& returnType)
{
    const QString t = normalizeType(returnType);
    return t.isEmpty() || t == QLatin1String("void");
}

QVariant ViewerCore::coerceArg(const QString& typeName, const QString& text)
{
    const QString t = normalizeType(typeName);

    if (t == QLatin1String("int") || t == QLatin1String("long")
        || t == QLatin1String("qint32") || t == QLatin1String("qint64")
        || t == QLatin1String("int64_t") || t == QLatin1String("int32_t")) {
        return QVariant(text.toLongLong());
    }
    if (t == QLatin1String("uint") || t == QLatin1String("unsigned")
        || t == QLatin1String("quint32") || t == QLatin1String("quint64")
        || t == QLatin1String("uint64_t") || t == QLatin1String("uint32_t")) {
        return QVariant(text.toULongLong());
    }
    if (t == QLatin1String("double") || t == QLatin1String("float")
        || t == QLatin1String("qreal")) {
        return QVariant(text.toDouble());
    }
    if (t == QLatin1String("bool")) {
        const QString v = text.trimmed().toLower();
        return QVariant(v == QLatin1String("true") || v == QLatin1String("1")
                        || v == QLatin1String("yes"));
    }
    // QString, QByteArray, QVariant, and anything else: pass through as text.
    return QVariant(text);
}

ViewerCore::ModuleInfo ViewerCore::loadModule(const QString& path)
{
    ModuleInfo info;

    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        info.error = QStringLiteral("Module file not found: %1").arg(path);
        m_module = info;
        return info;
    }

    QString resolved = fileInfo.canonicalFilePath();
    if (resolved.isEmpty()) {
        resolved = fileInfo.absoluteFilePath();
    }
    info.path = resolved;

    // Introspect via ModuleLib::LogosModule. This transparently handles both
    // legacy Q_INVOKABLE plugins and modern universal/cdylib provider plugins
    // (createProviderObject()->getMethods()), so we no longer walk QMetaObject
    // ourselves (which returns nothing for universal modules).
    QString loadError;
    ModuleLib::LogosModule loaded =
        ModuleLib::LogosModule::loadFromPath(resolved, &loadError);

    if (!loaded.isValid()) {
        info.error = loadError.isEmpty()
            ? QStringLiteral("Failed to load module plugin")
            : loadError;
        m_module = info;
        return info;
    }

    const ModuleLib::ModuleMetadata& meta = loaded.metadata();
    info.name = meta.name;
    info.version = meta.version;
    info.description = meta.description;
    info.type = meta.type;

    // Fall back to the file base name if the plugin carries no name metadata.
    if (info.name.isEmpty()) {
        QString base = fileInfo.baseName();
        if (base.endsWith(QLatin1String("_plugin"))) {
            base.chop(int(qstrlen("_plugin")));
        }
        info.name = base;
    }

    info.methods = loaded.getMethods(/*excludeBaseClass=*/true);

    // Events ride inside the provider's getMethods() tagged type "event";
    // getEventsAsJson() splits them out. Re-parse into MethodInfo for uniformity.
    const QJsonArray eventsJson = loaded.getEventsAsJson();
    for (const QJsonValue& v : eventsJson) {
        const QJsonObject eo = v.toObject();
        ModuleLib::MethodInfo ev;
        ev.name = eo.value(QStringLiteral("name")).toString();
        ev.signature = eo.value(QStringLiteral("signature")).toString();
        ev.returnType = eo.value(QStringLiteral("returnType")).toString();
        ev.description = eo.value(QStringLiteral("description")).toString();
        const QJsonArray params = eo.value(QStringLiteral("parameters")).toArray();
        for (const QJsonValue& pv : params) {
            const QJsonObject po = pv.toObject();
            ModuleLib::ParameterInfo p;
            p.name = po.value(QStringLiteral("name")).toString();
            p.type = po.value(QStringLiteral("type")).toString();
            ev.parameters.push_back(p);
        }
        info.events.push_back(ev);
    }

    info.ok = true;

    // Adopt as the current target; reset any prior IPC-load state.
    m_loaded = std::move(loaded);
    m_module = info;
    m_moduleLoadedInCore = false;
    return info;
}

bool ViewerCore::startCore(QString* error)
{
    if (m_coreInitialized) {
        return true;
    }

    QString dir = m_modulesDir;
    if (dir.isEmpty()) {
        dir = QDir::cleanPath(QCoreApplication::applicationDirPath()
                              + QStringLiteral("/../modules"));
    }

    // QCoreApplication must already exist (created by main()). logos_core_init is
    // part of the documented startup contract; it ignores argc/argv today, and the
    // Qt application object — not these args — is what the runtime actually needs.
    logos_core_init(0, nullptr);

    logos_core_add_modules_dir(dir.toUtf8().constData());
    logos_core_start();
    m_coreInitialized = true;

    // Preload supporting modules (e.g. capability_module for the auth handshake).
    for (const QString& mod : m_preloadModules) {
        logos_core_load_module(mod.toUtf8().constData(), /*with_dependencies=*/true);
    }

    if (!m_logosAPI) {
        m_logosAPI = new LogosAPI("module_viewer", this);
    }

    Q_UNUSED(error);
    return true;
}

bool ViewerCore::ensureLoaded(QString* error)
{
    if (!m_module.ok) {
        if (error) *error = QStringLiteral("No module loaded");
        return false;
    }

    if (!startCore(error)) {
        return false;
    }

    if (!m_moduleLoadedInCore) {
        char* processedName =
            logos_core_process_module(m_module.path.toUtf8().constData());
        if (!processedName) {
            if (error) {
                *error = QStringLiteral("logos_core_process_module failed for %1")
                             .arg(m_module.path);
            }
            return false;
        }
        const QString moduleName = QString::fromUtf8(processedName);
        free(processedName);

        // Prefer the name liblogos derived from the plugin metadata; keep ours
        // as a fallback only.
        if (!moduleName.isEmpty()) {
            m_module.name = moduleName;
        }

        const int loaded =
            logos_core_load_module(m_module.name.toUtf8().constData(),
                                   /*with_dependencies=*/true);
        if (loaded != 1) {
            if (error) {
                *error = QStringLiteral("logos_core_load_module failed for %1")
                             .arg(m_module.name);
            }
            return false;
        }
        m_moduleLoadedInCore = true;
    }

    // Wait for the IPC client replica to connect before the first call. The QRO
    // link comes up asynchronously, so poll isConnected() with a bounded spin
    // (mirrors clientReady() in logos-package-manager-ui's backend).
    LogosAPIClient* client = m_logosAPI->getClient(m_module.name);
    if (!client) {
        if (error) {
            *error = QStringLiteral("Failed to get API client for %1")
                         .arg(m_module.name);
        }
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (!client->isConnected() && timer.elapsed() < 10000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    // Not fatal if still not "connected" — some transports report lazily; the
    // call itself will surface a real error. We proceed either way.

    return true;
}

ViewerCore::CallResult ViewerCore::callMethod(const QString& methodName,
                                              const QVariantList& args)
{
    CallResult result;

    QString error;
    if (!ensureLoaded(&error)) {
        result.error = error;
        return result;
    }

    LogosAPIClient* client = m_logosAPI->getClient(m_module.name);
    if (!client) {
        result.error = QStringLiteral("Failed to get API client for %1")
                           .arg(m_module.name);
        return result;
    }

    // Find the method's declared return type (for void detection).
    QString returnType;
    for (const ModuleLib::MethodInfo& m : m_module.methods) {
        if (m.name == methodName) {
            returnType = m.returnType;
            break;
        }
    }

    const QVariant value =
        client->invokeRemoteMethod(m_module.name, methodName, args);

    result.ok = true;
    result.value = value;
    result.isVoid = isVoidType(returnType);
    return result;
}

ViewerCore::CallResult ViewerCore::callExpression(const QString& expression)
{
    CallResult result;

    const QString expr = expression.trimmed();
    const int open = expr.indexOf(QLatin1Char('('));
    const int close = expr.lastIndexOf(QLatin1Char(')'));

    QString methodName;
    QStringList rawArgs;
    if (open >= 0 && close > open) {
        methodName = expr.left(open).trimmed();
        const QString inner = expr.mid(open + 1, close - open - 1).trimmed();
        if (!inner.isEmpty()) {
            // Simple comma split — sufficient for the scalar args the viewer
            // deals with. Each part is trimmed individually.
            const QStringList parts = inner.split(QLatin1Char(','));
            for (const QString& p : parts) {
                rawArgs << p.trimmed();
            }
        }
    } else {
        // No parens: treat the whole thing as a no-arg method name.
        methodName = expr;
    }

    if (methodName.isEmpty()) {
        result.error = QStringLiteral("Could not parse method from: %1").arg(expression);
        return result;
    }

    // Need the method signature to coerce args; this also loads the module.
    QString error;
    if (!ensureLoaded(&error)) {
        result.error = error;
        return result;
    }

    const ModuleLib::MethodInfo* method = nullptr;
    for (const ModuleLib::MethodInfo& m : m_module.methods) {
        if (m.name == methodName) {
            method = &m;
            break;
        }
    }
    if (!method) {
        result.error = QStringLiteral("Method '%1' not found on module '%2'")
                           .arg(methodName, m_module.name);
        return result;
    }

    QVariantList args;
    for (int i = 0; i < rawArgs.size(); ++i) {
        QString text = rawArgs.at(i);

        // @filename loads file content as the argument (logoscore convention).
        if (text.startsWith(QLatin1Char('@'))) {
            const QString fileName = text.mid(1);
            QFile f(fileName);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                text = QString::fromUtf8(f.readAll());
                f.close();
            } else {
                result.error = QStringLiteral("Could not read file: %1").arg(fileName);
                return result;
            }
        }

        QString paramType = QStringLiteral("QString");
        if (i < int(method->parameters.size())) {
            paramType = method->parameters[size_t(i)].type;
        }
        args << coerceArg(paramType, text);
    }

    return callMethod(methodName, args);
}

bool ViewerCore::subscribeEvent(const QString& eventName,
                                EventHandler handler,
                                QString* error)
{
    if (eventName.isEmpty()) {
        if (error) *error = QStringLiteral("Event name cannot be empty");
        return false;
    }

    QString err;
    if (!ensureLoaded(&err)) {
        if (error) *error = err;
        return false;
    }

    LogosAPIClient* client = m_logosAPI->getClient(m_module.name);
    if (!client) {
        if (error) {
            *error = QStringLiteral("Failed to get API client for %1").arg(m_module.name);
        }
        return false;
    }

    LogosObject* obj = client->requestObject(m_module.name);
    if (!obj) {
        if (error) {
            *error = QStringLiteral("Failed to get remote object for %1").arg(m_module.name);
        }
        return false;
    }

    // 3-arg onEvent: (originObject, eventName, callback). The handler runs on
    // the Qt event loop.
    client->onEvent(obj, eventName, std::move(handler));
    m_eventObjects.push_back(obj);
    return true;
}
