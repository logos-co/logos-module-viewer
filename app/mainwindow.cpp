#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPluginLoader>
#include <QMetaObject>
#include <QMetaMethod>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QJsonObject>
#include <QFont>
#include <QStringList>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFrame>
#include <iostream>

#include "logos_api.h"
#include "logos_api_client.h"

extern "C" {
    void logos_core_set_plugins_dir(const char* plugins_dir);
    void logos_core_start();
    void logos_core_cleanup();
    char* logos_core_process_plugin(const char* plugin_path);
    int logos_core_load_plugin(const char* plugin_name);
}

MainWindow::MainWindow(const QString& modulePath, QWidget *parent)
    : QMainWindow(parent)
    , m_modulePath(modulePath)
    , m_headerLabel(nullptr)
    , m_methodsTree(nullptr)
    , m_pluginLoader(nullptr)
    , m_pluginInstance(nullptr)
    , m_coreInitialized(false)
    , m_logosAPI(nullptr)
{
    setupUi();

    if (!m_modulePath.isEmpty()) {
        loadModule(m_modulePath);
    }
}

MainWindow::~MainWindow()
{
    if (m_pluginLoader) {
        m_pluginLoader->unload();
        delete m_pluginLoader;
    }
    if (m_logosAPI) {
        delete m_logosAPI;
    }
    if (m_coreInitialized) {
        logos_core_cleanup();
    }
}

void MainWindow::setupUi()
{
    setWindowTitle("Logos Module Viewer");
    resize(1000, 800);

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
    m_methodsTree->setRootIsDecorated(true);
    m_methodsTree->setSortingEnabled(false);
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

QWidget* MainWindow::createMethodForm(const QMetaMethod& method, int methodIndex)
{
    QWidget* formContainer = new QWidget();
    formContainer->setStyleSheet("background-color: #f8f9fa; border-radius: 4px;");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(formContainer);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    QFormLayout* formLayout = new QFormLayout();
    formLayout->setSpacing(8);
    formLayout->setLabelAlignment(Qt::AlignRight);

    QByteArrayList paramNames = method.parameterNames();
    for (int p = 0; p < method.parameterCount(); ++p) {
        QString paramType = QString::fromUtf8(method.parameterTypeName(p));
        QString paramName;
        if (p < paramNames.size() && !paramNames.at(p).isEmpty()) {
            paramName = QString::fromUtf8(paramNames.at(p));
        } else {
            paramName = QString("param%1").arg(p);
        }

        QWidget* inputWidget = nullptr;
        QString normalizedType = paramType;
        normalizedType.remove("const ");
        normalizedType.remove("&");
        normalizedType.remove("*");
        normalizedType = normalizedType.trimmed();

        if (normalizedType == "int") {
            QSpinBox* spin = new QSpinBox();
            spin->setRange(-2147483647, 2147483647);
            spin->setObjectName(QString("param_%1").arg(p));
            spin->setProperty("paramType", paramType);
            inputWidget = spin;
        } else if (normalizedType == "double" || normalizedType == "float") {
            QDoubleSpinBox* spin = new QDoubleSpinBox();
            spin->setRange(-1e10, 1e10);
            spin->setDecimals(6);
            spin->setObjectName(QString("param_%1").arg(p));
            spin->setProperty("paramType", paramType);
            inputWidget = spin;
        } else if (normalizedType == "bool") {
            QCheckBox* check = new QCheckBox();
            check->setObjectName(QString("param_%1").arg(p));
            check->setProperty("paramType", paramType);
            inputWidget = check;
        } else {
            QLineEdit* edit = new QLineEdit();
            edit->setPlaceholderText(paramType);
            edit->setObjectName(QString("param_%1").arg(p));
            edit->setProperty("paramType", paramType);
            inputWidget = edit;
        }

        inputWidget->setStyleSheet(
            "QLineEdit, QSpinBox, QDoubleSpinBox {"
            "  padding: 6px 8px;"
            "  border: 1px solid #ced4da;"
            "  border-radius: 4px;"
            "  background: white;"
            "  color: #1a1a2e;"
            "  min-width: 200px;"
            "}"
            "QCheckBox { padding: 4px; color: #1a1a2e; }"
        );

        QString labelText = QString("<b>%1</b> <span style='color: #6c757d;'>(%2)</span>").arg(paramName, paramType);
        QLabel* label = new QLabel(labelText);
        formLayout->addRow(label, inputWidget);
    }

    if (method.parameterCount() == 0) {
        QLabel* noParams = new QLabel("<i style='color: #6c757d;'>No parameters</i>");
        formLayout->addRow(noParams);
    }

    mainLayout->addLayout(formLayout);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    QPushButton* callButton = new QPushButton("Call Method");
    callButton->setObjectName("callButton");
    callButton->setProperty("methodIndex", methodIndex);
    callButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #27ae60;"
        "  color: white;"
        "  border: none;"
        "  padding: 8px 16px;"
        "  border-radius: 4px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: #219a52; }"
        "QPushButton:pressed { background-color: #1e8449; }"
    );
    connect(callButton, &QPushButton::clicked, this, &MainWindow::onCallMethod);

    buttonLayout->addWidget(callButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    QFrame* resultFrame = new QFrame();
    resultFrame->setObjectName("resultFrame");
    resultFrame->setStyleSheet(
        "QFrame {"
        "  background-color: #ffffff;"
        "  border: 1px solid #dee2e6;"
        "  border-radius: 4px;"
        "  padding: 8px;"
        "}"
    );
    QVBoxLayout* resultLayout = new QVBoxLayout(resultFrame);
    resultLayout->setContentsMargins(8, 8, 8, 8);

    QLabel* resultTitle = new QLabel("<b>Result:</b>");
    resultLayout->addWidget(resultTitle);

    QLabel* resultLabel = new QLabel("<i style='color: #6c757d;'>Not called yet</i>");
    resultLabel->setObjectName("resultLabel");
    resultLabel->setWordWrap(true);
    resultLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    resultLayout->addWidget(resultLabel);

    mainLayout->addWidget(resultFrame);

    return formContainer;
}

void MainWindow::onCallMethod()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;

    int methodIndex = button->property("methodIndex").toInt();
    QWidget* formWidget = button->parentWidget()->parentWidget();
    invokeMethod(methodIndex, formWidget);
}

void MainWindow::invokeMethod(int methodIndex, QWidget* formWidget)
{
    if (!m_pluginInstance || !m_logosAPI) {
        QLabel* resultLabel = formWidget->findChild<QLabel*>("resultLabel");
        if (resultLabel) {
            resultLabel->setText("<span style='color: #dc3545;'><b>Error:</b> LogosAPI not initialized</span>");
        }
        return;
    }

    const QMetaObject* metaObject = m_pluginInstance->metaObject();
    QMetaMethod method = metaObject->method(methodIndex);

    QLabel* resultLabel = formWidget->findChild<QLabel*>("resultLabel");
    if (!resultLabel) return;

    QVariantList args;
    for (int p = 0; p < method.parameterCount(); ++p) {
        QString paramType = QString::fromUtf8(method.parameterTypeName(p));
        QString normalizedType = paramType;
        normalizedType.remove("const ");
        normalizedType.remove("&");
        normalizedType.remove("*");
        normalizedType = normalizedType.trimmed();

        QWidget* inputWidget = formWidget->findChild<QWidget*>(QString("param_%1").arg(p));

        if (normalizedType == "int") {
            QSpinBox* spin = qobject_cast<QSpinBox*>(inputWidget);
            args.append(spin ? spin->value() : 0);
        } else if (normalizedType == "double") {
            QDoubleSpinBox* spin = qobject_cast<QDoubleSpinBox*>(inputWidget);
            args.append(spin ? spin->value() : 0.0);
        } else if (normalizedType == "float") {
            QDoubleSpinBox* spin = qobject_cast<QDoubleSpinBox*>(inputWidget);
            args.append(spin ? static_cast<float>(spin->value()) : 0.0f);
        } else if (normalizedType == "bool") {
            QCheckBox* check = qobject_cast<QCheckBox*>(inputWidget);
            args.append(check ? check->isChecked() : false);
        } else {
            QLineEdit* edit = qobject_cast<QLineEdit*>(inputWidget);
            args.append(edit ? edit->text() : QString());
        }
    }

    QString methodName = QString::fromUtf8(method.name());
    
    resultLabel->setText("<i style='color: #6c757d;'>Calling remote method...</i>");
    QCoreApplication::processEvents();

    std::cout << "Invoking remote method: " << m_currentModuleName.toStdString() 
              << "." << methodName.toStdString() << " with " << args.size() << " args" << std::endl;
    
    LogosAPIClient* client = m_logosAPI->getClient(m_currentModuleName);
    if (!client) {
        resultLabel->setText("<span style='color: #dc3545;'><b>Error:</b> Failed to get API client</span>");
        return;
    }

    QVariant result = client->invokeRemoteMethod(m_currentModuleName, methodName, args);
    
    QString returnType = QString::fromUtf8(method.typeName());
    QString normalizedReturn = returnType;
    normalizedReturn.remove("const ");
    normalizedReturn.remove("&");
    normalizedReturn.remove("*");
    normalizedReturn = normalizedReturn.trimmed();

    if (normalizedReturn.isEmpty() || normalizedReturn == "void") {
        resultLabel->setText("<span style='color: #27ae60;'>Method called successfully (void return)</span>");
    } else {
        QString resultText = result.toString();
        if (resultText.isEmpty() && result.canConvert<QStringList>()) {
            resultText = result.toStringList().join(", ");
        }
        if (resultText.isEmpty() && result.isValid()) {
            resultText = QString("(%1)").arg(result.typeName());
        }
        if (resultText.isEmpty()) {
            resultText = "(empty or null result)";
        }
        resultLabel->setText(QString("<span style='color: #27ae60;'><b>Result:</b></span> %1").arg(resultText.toHtmlEscaped()));
    }
}

void MainWindow::loadModule(const QString& path)
{
    m_methodsTree->clear();
    m_itemToMethodIndex.clear();

    if (m_pluginLoader) {
        m_pluginLoader->unload();
        delete m_pluginLoader;
        m_pluginLoader = nullptr;
        m_pluginInstance = nullptr;
    }

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

    if (!m_coreInitialized) {
        QString modulesDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../modules");
        std::cout << "Setting modules directory to: " << modulesDir.toStdString() << std::endl;
        logos_core_set_plugins_dir(modulesDir.toUtf8().constData());
        logos_core_start();
        std::cout << "Logos Core started" << std::endl;
        m_coreInitialized = true;

        m_logosAPI = new LogosAPI("module_viewer", this);
        std::cout << "LogosAPI initialized" << std::endl;
    }

    // Extract module name from file name (e.g., "package_manager_plugin.dylib" -> "package_manager")
    QString baseName = fileInfo.baseName();
    if (baseName.endsWith("_plugin")) {
        baseName.chop(7); // Remove "_plugin" suffix
    }
    m_currentModuleName = baseName;
    std::cout << "Module name: " << m_currentModuleName.toStdString() << std::endl;

    std::cout << "Processing plugin: " << resolvedPath.toStdString() << std::endl;
    char* pluginName = logos_core_process_plugin(resolvedPath.toUtf8().constData());
    if (pluginName) {
        std::cout << "Plugin processed, name: " << pluginName << std::endl;
        bool loaded = logos_core_load_plugin(pluginName);
        if (loaded) {
            std::cout << "Plugin loaded successfully via Logos Core" << std::endl;
        } else {
            std::cout << "Warning: Failed to load plugin via Logos Core" << std::endl;
        }
        free(pluginName);
    } else {
        std::cout << "Warning: Failed to process plugin via Logos Core" << std::endl;
    }

    m_pluginLoader = new QPluginLoader(resolvedPath, this);
    m_pluginInstance = m_pluginLoader->instance();

    if (!m_pluginInstance) {
        m_headerLabel->setText("<b>Error:</b> Failed to load module<br><span style='color: #868e96;'>" + m_pluginLoader->errorString() + "</span>");
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
        delete m_pluginLoader;
        m_pluginLoader = nullptr;
        return;
    }

    QString moduleName;
    QJsonObject metaData = m_pluginLoader->metaData();
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
    headerText += QString("<br><span style='color: #3498db; font-size: 11px;'>Remote module: %1</span>").arg(m_currentModuleName);
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

    const QMetaObject* metaObject = m_pluginInstance->metaObject();
    for (int i = 0; i < metaObject->methodCount(); ++i) {
        QMetaMethod method = metaObject->method(i);

        if (method.enclosingMetaObject() != metaObject) {
            continue;
        }

        if (method.methodType() == QMetaMethod::Signal) {
            continue;
        }

        QString methodType;
        switch (method.methodType()) {
            case QMetaMethod::Method:
                methodType = "Method";
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
        m_itemToMethodIndex[item] = i;

        QTreeWidgetItem* formItem = new QTreeWidgetItem(item);
        formItem->setFirstColumnSpanned(true);
        
        QWidget* formWidget = createMethodForm(method, i);
        m_methodsTree->setItemWidget(formItem, 0, formWidget);
    }

    m_methodsTree->expandAll();
    setWindowTitle(QString("Logos Module Viewer - %1").arg(moduleName));
}
