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
    QLineEdit* m_eventNameInput;
    QTextEdit* m_eventLog;
    QMap<QString, QObject*> m_eventSubscriptions;
};

#endif // MAINWINDOW_H
