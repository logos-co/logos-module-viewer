#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QMap>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPluginLoader;
class QWidget;
class QMetaMethod;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(const QString& modulePath = QString(), QWidget *parent = nullptr);
    ~MainWindow();

    void loadModule(const QString& path);

private slots:
    void onCallMethod();

private:
    void setupUi();
    QWidget* createMethodForm(const QMetaMethod& method, int methodIndex);
    void invokeMethod(int methodIndex, QWidget* formWidget);

    QString m_modulePath;
    QLabel* m_headerLabel;
    QTreeWidget* m_methodsTree;
    QPluginLoader* m_pluginLoader;
    QObject* m_pluginInstance;
    QMap<QTreeWidgetItem*, int> m_itemToMethodIndex;
};

#endif // MAINWINDOW_H

