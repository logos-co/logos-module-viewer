#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

class QTreeWidget;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(const QString& modulePath = QString(), QWidget *parent = nullptr);
    ~MainWindow();

    void loadModule(const QString& path);

private:
    void setupUi();

    QString m_modulePath;
    QLabel* m_headerLabel;
    QTreeWidget* m_methodsTree;
};

#endif // MAINWINDOW_H

