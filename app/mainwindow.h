#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QMap>
#include <QSet>

#include "viewer_core.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QWidget;
class QLineEdit;
class QTextEdit;

// MainWindow is the Qt Widgets front-end. It is a thin view over ViewerCore:
// introspection, module loading, IPC calls and event subscription all live in
// ViewerCore, so this class only builds and drives the UI.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString& modulePath = QString(),
                        const QString& modulesDir = QString(),
                        QWidget* parent = nullptr);
    ~MainWindow() override;

    void loadModule(const QString& path);

private slots:
    void onCallMethod();
    void onSubscribeEvent();

private:
    void setupUi();
    QWidget* createMethodForm(const ModuleLib::MethodInfo& method);
    void invokeMethod(const QString& methodName, QWidget* formWidget);
    void appendEventToLog(const QString& eventName, const QVariantList& data);
    void showHeaderError(const QString& html);

    ViewerCore* m_core;
    QString m_modulePath;

    QLabel* m_headerLabel = nullptr;
    QTreeWidget* m_methodsTree = nullptr;
    QLineEdit* m_eventNameInput = nullptr;
    QTextEdit* m_eventLog = nullptr;
    QSet<QString> m_eventSubscriptions;
};

#endif // MAINWINDOW_H
