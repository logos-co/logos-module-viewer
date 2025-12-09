#include "mainwindow.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPluginLoader>
#include <QMetaObject>
#include <QMetaMethod>
#include <QFileInfo>
#include <QJsonObject>
#include <QFont>

MainWindow::MainWindow(const QString& modulePath, QWidget *parent)
    : QMainWindow(parent)
    , m_modulePath(modulePath)
    , m_headerLabel(nullptr)
    , m_methodsTree(nullptr)
{
    setupUi();

    if (!m_modulePath.isEmpty()) {
        loadModule(m_modulePath);
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    setWindowTitle("Logos Module Viewer");
    resize(900, 700);

    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_headerLabel = new QLabel("No module loaded", this);
    m_headerLabel->setWordWrap(true);
    m_headerLabel->setStyleSheet(
        "QLabel {"
        "  font-family: -apple-system, 'Segoe UI', sans-serif;"
        "  font-size: 14px;"
        "  color: #495057;"
        "  padding: 12px 16px;"
        "  background-color: #f8f9fa;"
        "  border: 1px solid #e9ecef;"
        "  border-radius: 8px;"
        "  line-height: 1.5;"
        "}"
    );
    layout->addWidget(m_headerLabel);

    m_methodsTree = new QTreeWidget(this);
    m_methodsTree->setHeaderLabels({"Name", "Type", "Return Type", "Parameters"});
    m_methodsTree->setAlternatingRowColors(true);
    m_methodsTree->setRootIsDecorated(false);
    m_methodsTree->setSortingEnabled(true);
    m_methodsTree->header()->setStretchLastSection(true);
    m_methodsTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_methodsTree->setStyleSheet(
        "QTreeWidget {"
        "  font-family: 'SF Mono', 'Menlo', 'Monaco', monospace;"
        "  font-size: 13px;"
        "  border: 1px solid #ddd;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  alternate-background-color: #f8f9fa;"
        "}"
        "QTreeWidget::item {"
        "  padding: 6px 8px;"
        "  color: #1a1a2e;"
        "}"
        "QTreeWidget::item:selected {"
        "  background-color: #e3f2fd;"
        "  color: #1565c0;"
        "}"
        "QHeaderView::section {"
        "  background-color: #2d3436;"
        "  color: #ffffff;"
        "  padding: 10px 8px;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  border: none;"
        "  border-right: 1px solid #636e72;"
        "}"
        "QHeaderView::section:last {"
        "  border-right: none;"
        "}"
    );
    layout->addWidget(m_methodsTree);

    setCentralWidget(centralWidget);
}

void MainWindow::loadModule(const QString& path)
{
    m_methodsTree->clear();

    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        m_headerLabel->setText("<b>Error:</b> Module file not found<br><span style='color: #868e96;'>" + path + "</span>");
        m_headerLabel->setStyleSheet(
            "QLabel {"
            "  font-family: -apple-system, 'Segoe UI', sans-serif;"
            "  font-size: 14px;"
            "  color: #721c24;"
            "  padding: 16px;"
            "  background-color: #fff5f5;"
            "  border: 1px solid #f5c6cb;"
            "  border-left: 4px solid #dc3545;"
            "  border-radius: 8px;"
            "}"
        );
        return;
    }

    QString resolvedPath = fileInfo.canonicalFilePath();
    if (resolvedPath.isEmpty()) {
        resolvedPath = fileInfo.absoluteFilePath();
    }

    QPluginLoader loader(resolvedPath);
    QObject* plugin = loader.instance();

    if (!plugin) {
        m_headerLabel->setText("<b>Error:</b> Failed to load module<br><span style='color: #868e96;'>" + loader.errorString() + "</span>");
        m_headerLabel->setStyleSheet(
            "QLabel {"
            "  font-family: -apple-system, 'Segoe UI', sans-serif;"
            "  font-size: 14px;"
            "  color: #721c24;"
            "  padding: 16px;"
            "  background-color: #fff5f5;"
            "  border: 1px solid #f5c6cb;"
            "  border-left: 4px solid #dc3545;"
            "  border-radius: 8px;"
            "}"
        );
        return;
    }

    QString moduleName;
    QJsonObject metaData = loader.metaData();
    QJsonObject meta = metaData.value("MetaData").toObject();
    moduleName = meta.value("name").toString();
    QString moduleVersion = meta.value("version").toString();

    if (moduleName.isEmpty()) {
        moduleName = fileInfo.baseName();
    }

    QString headerText = QString("<b style='font-size: 16px; color: #1a1a2e;'>%1</b>").arg(moduleName);
    if (!moduleVersion.isEmpty()) {
        headerText += QString(" <span style='color: #6c757d; font-size: 13px;'>v%1</span>").arg(moduleVersion);
    }
    headerText += QString("<br><span style='color: #868e96; font-size: 12px;'>%1</span>").arg(resolvedPath);
    m_headerLabel->setText(headerText);
    m_headerLabel->setStyleSheet(
        "QLabel {"
        "  font-family: -apple-system, 'Segoe UI', sans-serif;"
        "  padding: 16px;"
        "  background-color: #ffffff;"
        "  border: 1px solid #dee2e6;"
        "  border-left: 4px solid #27ae60;"
        "  border-radius: 8px;"
        "}"
    );

    const QMetaObject* metaObject = plugin->metaObject();
    for (int i = 0; i < metaObject->methodCount(); ++i) {
        QMetaMethod method = metaObject->method(i);

        // Skip methods inherited from QObject base class
        if (method.enclosingMetaObject() != metaObject) {
            continue;
        }

        QString methodType;
        switch (method.methodType()) {
            case QMetaMethod::Method:
                methodType = "Method";
                break;
            case QMetaMethod::Signal:
                methodType = "Signal";
                break;
            case QMetaMethod::Slot:
                methodType = "Slot";
                break;
            case QMetaMethod::Constructor:
                methodType = "Constructor";
                break;
            default:
                methodType = "Unknown";
                break;
        }

        QString returnType = QString::fromUtf8(method.typeName());
        if (returnType.isEmpty()) {
            returnType = "void";
        }

        QStringList paramStrings;
        QByteArrayList paramNames = method.parameterNames();
        for (int p = 0; p < method.parameterCount(); ++p) {
            QString paramType = QString::fromUtf8(method.parameterTypeName(p));
            QString paramName;
            if (p < paramNames.size() && !paramNames.at(p).isEmpty()) {
                paramName = QString::fromUtf8(paramNames.at(p));
            } else {
                paramName = QString("param%1").arg(p);
            }
            paramStrings << QString("%1 %2").arg(paramType, paramName);
        }
        QString parameters = paramStrings.join(", ");
        if (parameters.isEmpty()) {
            parameters = "(none)";
        }

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, QString::fromUtf8(method.name()));
        item->setText(1, methodType);
        item->setText(2, returnType);
        item->setText(3, parameters);

        QColor typeColor;
        switch (method.methodType()) {
            case QMetaMethod::Signal:
                typeColor = QColor("#e67e22");
                break;
            case QMetaMethod::Slot:
                typeColor = QColor("#3498db");
                break;
            case QMetaMethod::Method:
                typeColor = QColor("#27ae60");
                break;
            default:
                typeColor = QColor("#95a5a6");
                break;
        }
        item->setForeground(1, typeColor);

        QFont nameFont = item->font(0);
        nameFont.setBold(true);
        item->setFont(0, nameFont);

        m_methodsTree->addTopLevelItem(item);
    }

    setWindowTitle(QString("Logos Module Viewer - %1").arg(moduleName));

    loader.unload();
}

