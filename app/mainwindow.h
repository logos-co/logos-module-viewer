#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPluginLoader;
class QWidget;
class LogosAPI;
class LogosObject;
class QLineEdit;
class QTextEdit;

struct MethodDescriptor {
    QString name;
    QString returnType;
    struct Param { QString type; QString name; };
    QList<Param> parameters;
};

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
    QWidget* createMethodForm(const MethodDescriptor& descriptor);
    void invokeMethod(const MethodDescriptor& descriptor, QWidget* formWidget);
    void appendEventToLog(const QString& eventName, const QVariantList& data);

    QString m_modulePath;
    QString m_currentModuleName;
    QLabel* m_headerLabel;
    QTreeWidget* m_methodsTree;
    QPluginLoader* m_pluginLoader;
    QObject* m_pluginInstance;
    QMap<QTreeWidgetItem*, MethodDescriptor> m_itemToMethod;
    bool m_coreInitialized;
    LogosAPI* m_logosAPI;
    QLineEdit* m_eventNameInput;
    QTextEdit* m_eventLog;
    QMap<QString, LogosObject*> m_eventSubscriptions;
};

#endif // MAINWINDOW_H
