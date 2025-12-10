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
#include <QTextEdit>
#include <QTextCursor>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QSplitter>
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
    , m_eventNameInput(nullptr)
    , m_eventLog(nullptr)
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
    centralWidget->setStyleSheet(
        "QWidget {"
        "  background-color: #1e1e1e;"
        "  color: #e0e0e0;"
        "}"
    );
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_headerLabel = new QLabel("No module loaded", this);
    m_headerLabel->setWordWrap(true);
    m_headerLabel->setStyleSheet(
        "QLabel {"
        "  font-family: -apple-system, 'Segoe UI', sans-serif;"
        "  font-size: 14px;"
        "  color: #b0b0b0;"
        "  padding: 12px 16px;"
        "  background-color: #2d2d2d;"
        "  border: 1px solid #3d3d3d;"
        "  border-radius: 8px;"
        "  line-height: 1.5;"
        "}"
    );
    layout->addWidget(m_headerLabel);

    QLabel* eventLabel = new QLabel("Event Subscription", this);
    eventLabel->setStyleSheet(
        "QLabel {"
        "  font-family: -apple-system, 'Segoe UI', sans-serif;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "  color: #e0e0e0;"
        "}"
    );
    layout->addWidget(eventLabel);

    QHBoxLayout* eventInputLayout = new QHBoxLayout();
    eventInputLayout->setSpacing(8);

    m_eventNameInput = new QLineEdit(this);
    m_eventNameInput->setPlaceholderText("Enter event name to subscribe...");
    m_eventNameInput->setStyleSheet(
        "QLineEdit {"
        "  padding: 8px 12px;"
        "  border: 1px solid #4d4d4d;"
        "  border-radius: 4px;"
        "  background: #1e1e1e;"
        "  color: #e0e0e0;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid #5a9;"
        "}"
    );
    eventInputLayout->addWidget(m_eventNameInput);

    QPushButton* subscribeButton = new QPushButton("Subscribe", this);
    subscribeButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #5a9;"
        "  color: #ffffff;"
        "  border: none;"
        "  padding: 8px 16px;"
        "  border-radius: 4px;"
        "  font-weight: 600;"
        "  min-width: 100px;"
        "}"
        "QPushButton:hover { background-color: #6bb; }"
        "QPushButton:pressed { background-color: #499; }"
    );
    connect(subscribeButton, &QPushButton::clicked, this, &MainWindow::onSubscribeEvent);
    eventInputLayout->addWidget(subscribeButton);

    layout->addLayout(eventInputLayout);

    m_eventLog = new QTextEdit(this);
    m_eventLog->setReadOnly(true);
    m_eventLog->setMinimumHeight(150);
    m_eventLog->setStyleSheet(
        "QTextEdit {"
        "  font-family: 'SF Mono', 'Menlo', 'Monaco', monospace;"
        "  font-size: 12px;"
        "  border: 1px solid #4d4d4d;"
        "  border-radius: 4px;"
        "  background-color: #1e1e1e;"
        "  color: #e0e0e0;"
        "  padding: 8px;"
        "}"
    );
    m_eventLog->setPlaceholderText("Event log will appear here...");

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
        "  border: 1px solid #3d3d3d;"
        "  border-radius: 6px;"
        "  background-color: #252525;"
        "  alternate-background-color: #2a2a2a;"
        "  color: #e0e0e0;"
        "}"
        "QTreeWidget::item {"
        "  padding: 6px 8px;"
        "  color: #e0e0e0;"
        "}"
        "QTreeWidget::item:selected {"
        "  background-color: #3a5a7a;"
        "  color: #ffffff;"
        "}"
        "QHeaderView::section {"
        "  background-color: #1a1a1a;"
        "  color: #e0e0e0;"
        "  padding: 10px 8px;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  border: none;"
        "  border-right: 1px solid #3d3d3d;"
        "}"
        "QHeaderView::section:last {"
        "  border-right: none;"
        "}"
    );

    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_eventLog);
    splitter->addWidget(m_methodsTree);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({200, 600});
    splitter->setStyleSheet(
        "QSplitter::handle {"
        "  background-color: #3d3d3d;"
        "  height: 4px;"
        "}"
        "QSplitter::handle:hover {"
        "  background-color: #5a9;"
        "}"
    );
    layout->addWidget(splitter);

    setCentralWidget(centralWidget);
}

QWidget* MainWindow::createMethodForm(const QMetaMethod& method, int methodIndex)
{
    QWidget* formContainer = new QWidget();
    formContainer->setObjectName("methodFormContainer");
    formContainer->setStyleSheet("background-color: #2a2a2a; border-radius: 4px;");
    
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
            "  border: 1px solid #4d4d4d;"
            "  border-radius: 4px;"
            "  background: #1e1e1e;"
            "  color: #e0e0e0;"
            "  min-width: 200px;"
            "}"
            "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {"
            "  border: 1px solid #5a9;"
            "}"
            "QCheckBox {"
            "  padding: 4px;"
            "  color: #e0e0e0;"
            "}"
            "QCheckBox::indicator {"
            "  width: 18px;"
            "  height: 18px;"
            "  border: 1px solid #4d4d4d;"
            "  border-radius: 3px;"
            "  background: #1e1e1e;"
            "}"
            "QCheckBox::indicator:checked {"
            "  background: #5a9;"
            "  border: 1px solid #5a9;"
            "}"
        );

        QString labelText = QString("<b style='color: #e0e0e0;'>%1</b> <span style='color: #888;'>(%2)</span>").arg(paramName, paramType);
        QLabel* label = new QLabel(labelText);
        label->setStyleSheet("color: #e0e0e0;");
        formLayout->addRow(label, inputWidget);
    }

    if (method.parameterCount() == 0) {
        QLabel* noParams = new QLabel("<i style='color: #888;'>No parameters</i>");
        noParams->setStyleSheet("color: #888;");
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
        "  background-color: #5a9;"
        "  color: #ffffff;"
        "  border: none;"
        "  padding: 8px 16px;"
        "  border-radius: 4px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: #6bb; }"
        "QPushButton:pressed { background-color: #499; }"
    );
    connect(callButton, &QPushButton::clicked, this, &MainWindow::onCallMethod);

    buttonLayout->addWidget(callButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    QFrame* resultFrame = new QFrame();
    resultFrame->setObjectName("resultFrame");
    resultFrame->setMinimumHeight(100);
    resultFrame->setStyleSheet(
        "QFrame {"
        "  background-color: #1e1e1e;"
        "  border: 1px solid #4d4d4d;"
        "  border-radius: 4px;"
        "  padding: 12px;"
        "}"
    );
    QVBoxLayout* resultLayout = new QVBoxLayout(resultFrame);
    resultLayout->setContentsMargins(12, 12, 12, 12);
    resultLayout->setSpacing(8);

    QLabel* resultTitle = new QLabel("<b style='color: #e0e0e0;'>Result:</b>");
    resultTitle->setStyleSheet("color: #e0e0e0;");
    resultLayout->addWidget(resultTitle);

    QLabel* resultLabel = new QLabel("<i style='color: #888;'>Not called yet</i>");
    resultLabel->setObjectName("resultLabel");
    resultLabel->setWordWrap(true);
    resultLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    resultLabel->setMinimumHeight(60);
    resultLabel->setStyleSheet(
        "QLabel {"
        "  padding: 8px;"
        "  background-color: #252525;"
        "  border-radius: 4px;"
        "  font-family: 'SF Mono', 'Menlo', 'Monaco', monospace;"
        "  font-size: 12px;"
        "}"
    );
    resultLayout->addWidget(resultLabel);

    mainLayout->addWidget(resultFrame);

    return formContainer;
}

void MainWindow::onCallMethod()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;

    int methodIndex = button->property("methodIndex").toInt();
    
    // Find the form container by traversing up the widget hierarchy
    QWidget* formWidget = button->parentWidget();
    while (formWidget && formWidget->objectName() != "methodFormContainer") {
        formWidget = formWidget->parentWidget();
    }
    
    if (!formWidget) {
        std::cout << "Error: Could not find form container widget" << std::endl;
        return;
    }
    
    invokeMethod(methodIndex, formWidget);
}

void MainWindow::invokeMethod(int methodIndex, QWidget* formWidget)
{
    if (!m_pluginInstance || !m_logosAPI) {
        QLabel* resultLabel = formWidget->findChild<QLabel*>("resultLabel", Qt::FindChildrenRecursively);
        if (resultLabel) {
            resultLabel->setText("<span style='color: #ff6b6b;'><b>Error:</b> LogosAPI not initialized</span>");
            resultLabel->update();
        }
        return;
    }

    const QMetaObject* metaObject = m_pluginInstance->metaObject();
    QMetaMethod method = metaObject->method(methodIndex);

    QLabel* resultLabel = formWidget->findChild<QLabel*>("resultLabel", Qt::FindChildrenRecursively);
    if (!resultLabel) {
        std::cout << "Error: Could not find resultLabel widget in formWidget: " << formWidget << std::endl;
        std::cout << "Form widget objectName: " << formWidget->objectName().toStdString() << std::endl;
        QList<QLabel*> allLabels = formWidget->findChildren<QLabel*>(Qt::FindChildrenRecursively);
        std::cout << "Found " << allLabels.size() << " labels in form widget" << std::endl;
        for (QLabel* label : allLabels) {
            std::cout << "  Label objectName: " << label->objectName().toStdString() << std::endl;
        }
        return;
    }
    
    std::cout << "Found resultLabel: " << resultLabel << ", text: " << resultLabel->text().toStdString() << std::endl;

    QVariantList args;
    for (int p = 0; p < method.parameterCount(); ++p) {
        QString paramType = QString::fromUtf8(method.parameterTypeName(p));
        QString normalizedType = paramType;
        normalizedType.remove("const ");
        normalizedType.remove("&");
        normalizedType.remove("*");
        normalizedType = normalizedType.trimmed();

        QString widgetName = QString("param_%1").arg(p);
        QWidget* inputWidget = formWidget->findChild<QWidget*>(widgetName, Qt::FindChildrenRecursively);

        if (!inputWidget) {
            std::cout << "Warning: Could not find widget " << widgetName.toStdString() << " for parameter " << p << std::endl;
            QList<QWidget*> allWidgets = formWidget->findChildren<QWidget*>(Qt::FindChildrenRecursively);
            std::cout << "Available widgets: ";
            for (QWidget* w : allWidgets) {
                if (!w->objectName().isEmpty()) {
                    std::cout << w->objectName().toStdString() << " ";
                }
            }
            std::cout << std::endl;
        } else {
            std::cout << "Found widget " << widgetName.toStdString() << ": " << inputWidget << std::endl;
        }

        if (normalizedType == "int") {
            QSpinBox* spin = qobject_cast<QSpinBox*>(inputWidget);
            if (spin) {
                args.append(spin->value());
                std::cout << "Parameter " << p << " (int): " << spin->value() << std::endl;
            } else {
                args.append(0);
            }
        } else if (normalizedType == "double") {
            QDoubleSpinBox* spin = qobject_cast<QDoubleSpinBox*>(inputWidget);
            if (spin) {
                args.append(spin->value());
                std::cout << "Parameter " << p << " (double): " << spin->value() << std::endl;
            } else {
                args.append(0.0);
            }
        } else if (normalizedType == "float") {
            QDoubleSpinBox* spin = qobject_cast<QDoubleSpinBox*>(inputWidget);
            if (spin) {
                args.append(static_cast<float>(spin->value()));
                std::cout << "Parameter " << p << " (float): " << spin->value() << std::endl;
            } else {
                args.append(0.0f);
            }
        } else if (normalizedType == "bool") {
            QCheckBox* check = qobject_cast<QCheckBox*>(inputWidget);
            if (check) {
                args.append(check->isChecked());
                std::cout << "Parameter " << p << " (bool): " << (check->isChecked() ? "true" : "false") << std::endl;
            } else {
                args.append(false);
            }
        } else {
            QLineEdit* edit = qobject_cast<QLineEdit*>(inputWidget);
            if (edit) {
                QString text = edit->text();
                args.append(text);
                std::cout << "Parameter " << p << " (string): " << text.toStdString() << std::endl;
            } else {
                args.append(QString());
            }
        }
    }

    QString methodName = QString::fromUtf8(method.name());
    
    resultLabel->setText("<i style='color: #888;'>Calling remote method...</i>");
    QCoreApplication::processEvents();

    std::cout << "Invoking remote method: " << m_currentModuleName.toStdString() 
              << "." << methodName.toStdString() << " with " << args.size() << " args" << std::endl;
    
    LogosAPIClient* client = m_logosAPI->getClient(m_currentModuleName);
    if (!client) {
        resultLabel->setText("<span style='color: #ff6b6b;'><b>Error:</b> Failed to get API client</span>");
        resultLabel->update();
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
        resultLabel->setText("<span style='color: #5a9;'>Method called successfully (void return)</span>");
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
        std::cout << "Result: " << resultText.toStdString() << std::endl;
        QString resultHtml = QString("<span style='color: #5a9;'><b>Result:</b></span> <span style='color: #e0e0e0;'>%1</span>").arg(resultText.toHtmlEscaped());
        resultLabel->setText(resultHtml);
        std::cout << "Set resultLabel text to: " << resultHtml.toStdString() << std::endl;
        std::cout << "resultLabel isVisible: " << resultLabel->isVisible() << ", isEnabled: " << resultLabel->isEnabled() << std::endl;
        resultLabel->show();
        resultLabel->update();
        resultLabel->repaint();
        QCoreApplication::processEvents();
    }
}

void MainWindow::onSubscribeEvent()
{
    if (!m_eventNameInput) {
        return;
    }

    QString eventName = m_eventNameInput->text().trimmed();
    if (eventName.isEmpty()) {
        appendEventToLog("Error", QVariantList() << "Event name cannot be empty");
        return;
    }

    if (m_eventSubscriptions.contains(eventName)) {
        appendEventToLog("Warning", QVariantList() << QString("Already subscribed to event: %1").arg(eventName));
        return;
    }

    if (m_currentModuleName.isEmpty() || !m_logosAPI) {
        appendEventToLog("Error", QVariantList() << "No module loaded or LogosAPI not initialized");
        return;
    }

    LogosAPIClient* client = m_logosAPI->getClient(m_currentModuleName);
    if (!client) {
        appendEventToLog("Error", QVariantList() << QString("Failed to get API client for module: %1").arg(m_currentModuleName));
        return;
    }

    QObject* replica = client->requestObject(m_currentModuleName);
    if (!replica) {
        appendEventToLog("Error", QVariantList() << QString("Failed to get replica object for module: %1").arg(m_currentModuleName));
        return;
    }

    client->onEvent(replica, nullptr, eventName, [this](const QString& name, const QVariantList& data) {
        appendEventToLog(name, data);
    });

    m_eventSubscriptions[eventName] = replica;
    appendEventToLog("Info", QVariantList() << QString("Subscribed to event: %1").arg(eventName));
    m_eventNameInput->clear();
}

void MainWindow::appendEventToLog(const QString& eventName, const QVariantList& data)
{
    if (!m_eventLog) {
        return;
    }

    QJsonObject eventObj;
    eventObj["event"] = eventName;
    eventObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray dataArray;
    for (const QVariant& v : data) {
        dataArray.append(QJsonValue::fromVariant(v));
    }
    eventObj["data"] = dataArray;

    QJsonDocument doc(eventObj);
    QString jsonString = doc.toJson(QJsonDocument::Indented);

    m_eventLog->append(jsonString);
    
    QTextCursor cursor = m_eventLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_eventLog->setTextCursor(cursor);
}

void MainWindow::loadModule(const QString& path)
{
    m_methodsTree->clear();
    m_itemToMethodIndex.clear();

    m_eventSubscriptions.clear();
    if (m_eventLog) {
        m_eventLog->clear();
    }

    if (m_pluginLoader) {
        m_pluginLoader->unload();
        delete m_pluginLoader;
        m_pluginLoader = nullptr;
        m_pluginInstance = nullptr;
    }

    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        m_headerLabel->setText("<b style='color: #ff6b6b;'>Error:</b> Module file not found<br><span style='color: #888;'>" + path + "</span>");
        m_headerLabel->setStyleSheet(
            "QLabel {"
            "  font-family: -apple-system, 'Segoe UI', sans-serif;"
            "  font-size: 14px;"
            "  color: #ff6b6b;"
            "  padding: 16px;"
            "  background-color: #3a2525;"
            "  border: 1px solid #5a3535;"
            "  border-left: 4px solid #ff6b6b;"
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
        m_headerLabel->setText("<b style='color: #ff6b6b;'>Error:</b> Failed to load module<br><span style='color: #888;'>" + m_pluginLoader->errorString() + "</span>");
        m_headerLabel->setStyleSheet(
            "QLabel {"
            "  font-family: -apple-system, 'Segoe UI', sans-serif;"
            "  font-size: 14px;"
            "  color: #ff6b6b;"
            "  padding: 16px;"
            "  background-color: #3a2525;"
            "  border: 1px solid #5a3535;"
            "  border-left: 4px solid #ff6b6b;"
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

    QString headerText = QString("<b style='font-size: 16px; color: #e0e0e0;'>%1</b>").arg(moduleName);
    if (!moduleVersion.isEmpty()) {
        headerText += QString(" <span style='color: #888; font-size: 13px;'>v%1</span>").arg(moduleVersion);
    }
    headerText += QString("<br><span style='color: #888; font-size: 12px;'>%1</span>").arg(resolvedPath);
    headerText += QString("<br><span style='color: #5a9; font-size: 11px;'>Remote module: %1</span>").arg(m_currentModuleName);
    m_headerLabel->setText(headerText);
    m_headerLabel->setStyleSheet(
        "QLabel {"
        "  font-family: -apple-system, 'Segoe UI', sans-serif;"
        "  padding: 16px;"
        "  background-color: #2d2d2d;"
        "  border: 1px solid #3d3d3d;"
        "  border-left: 4px solid #5a9;"
        "  border-radius: 8px;"
        "  color: #e0e0e0;"
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
                typeColor = QColor("#6bb");
                break;
            case QMetaMethod::Method:
                typeColor = QColor("#5a9");
                break;
            default:
                typeColor = QColor("#888");
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

