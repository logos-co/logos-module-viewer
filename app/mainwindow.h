#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QMap>
#include <QVariant>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPluginLoader;
class QWidget;
class QMetaMethod;
class LogosAPI;
struct logos_consumer;
struct logos_runtime;
// LogosAPIClient::requestObject returns LogosObject*, not QObject*. It is a
// plain class (logos_object.h), NOT a QObject, so it cannot be stored as one.
class LogosObject;
class QLineEdit;
class QTextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(const QString& modulePath = QString(), QWidget *parent = nullptr);
    ~MainWindow();

    void loadModule(const QString& path);

private slots:
    void onCallMethod();
    void onSubscribeEvent();

private:
    void setupUi();
    QWidget* createMethodForm(const QMetaMethod& method, int methodIndex);
    void invokeMethod(int methodIndex, QWidget* formWidget);
    void appendEventToLog(const QString& eventName, const QVariantList& data);

    QString m_modulePath;
    QString m_currentModuleName;
    QLabel* m_headerLabel;
    QTreeWidget* m_methodsTree;
    QPluginLoader* m_pluginLoader;
    QObject* m_pluginInstance;
    QMap<QTreeWidgetItem*, int> m_itemToMethodIndex;
    bool m_coreInitialized;
    LogosAPI* m_logosAPI;
    // The runtime runs in a process of its own; m_shell is its binding, owned by it.
    logos_runtime* m_runtime = nullptr;
    logos_consumer* m_shell = nullptr;
    QLineEdit* m_eventNameInput;
    QTextEdit* m_eventLog;
    // Holds the handle only: this map is used for contains() / assignment /
    // clear() and never dereferences the pointer, which is why retyping it from
    // QObject* is a type change and not a behaviour change.
    QMap<QString, LogosObject*> m_eventSubscriptions;
};

#endif // MAINWINDOW_H
