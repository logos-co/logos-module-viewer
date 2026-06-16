#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QHeaderView>
#include <QFileInfo>
#include <QCoreApplication>
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
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QSplitter>

namespace {

const char* kHeaderOkStyle =
    "QLabel {"
    "  font-family: -apple-system, 'Segoe UI', sans-serif;"
    "  padding: 16px;"
    "  background-color: #2d2d2d;"
    "  border: 1px solid #3d3d3d;"
    "  border-left: 4px solid #5a9;"
    "  border-radius: 8px;"
    "  color: #e0e0e0;"
    "}";

const char* kHeaderErrorStyle =
    "QLabel {"
    "  font-family: -apple-system, 'Segoe UI', sans-serif;"
    "  font-size: 14px;"
    "  color: #ff6b6b;"
    "  padding: 16px;"
    "  background-color: #3a2525;"
    "  border: 1px solid #5a3535;"
    "  border-left: 4px solid #ff6b6b;"
    "  border-radius: 8px;"
    "}";

const char* kInputStyle =
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
    "}";

const char* kButtonStyle =
    "QPushButton {"
    "  background-color: #5a9;"
    "  color: #ffffff;"
    "  border: none;"
    "  padding: 8px 16px;"
    "  border-radius: 4px;"
    "  font-weight: 600;"
    "}"
    "QPushButton:hover { background-color: #6bb; }"
    "QPushButton:pressed { background-color: #499; }";

// Normalize a parameter type the same way ViewerCore does, for picking a widget.
QString normalizeType(const QString& type)
{
    QString t = type;
    t.remove(QStringLiteral("const "));
    t.remove(QLatin1Char('&'));
    t.remove(QLatin1Char('*'));
    return t.trimmed();
}

} // namespace

MainWindow::MainWindow(const QString& modulePath, const QString& modulesDir, QWidget* parent)
    : QMainWindow(parent)
    , m_core(new ViewerCore(this))
    , m_modulePath(modulePath)
{
    if (!modulesDir.isEmpty()) {
        m_core->setModulesDir(modulesDir);
    }

    setupUi();

    if (!m_modulePath.isEmpty()) {
        loadModule(m_modulePath);
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Logos Module Viewer"));
    resize(1000, 800);

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setStyleSheet(
        "QWidget {"
        "  background-color: #1e1e1e;"
        "  color: #e0e0e0;"
        "}");
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_headerLabel = new QLabel(QStringLiteral("No module loaded"), this);
    m_headerLabel->setObjectName(QStringLiteral("headerLabel"));
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
        "}");
    layout->addWidget(m_headerLabel);

    QLabel* eventLabel = new QLabel(QStringLiteral("Event Subscription"), this);
    eventLabel->setStyleSheet(
        "QLabel {"
        "  font-family: -apple-system, 'Segoe UI', sans-serif;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "  color: #e0e0e0;"
        "}");
    layout->addWidget(eventLabel);

    QHBoxLayout* eventInputLayout = new QHBoxLayout();
    eventInputLayout->setSpacing(8);

    m_eventNameInput = new QLineEdit(this);
    m_eventNameInput->setObjectName(QStringLiteral("eventNameInput"));
    m_eventNameInput->setPlaceholderText(QStringLiteral("Enter event name to subscribe..."));
    m_eventNameInput->setStyleSheet(
        "QLineEdit {"
        "  padding: 8px 12px;"
        "  border: 1px solid #4d4d4d;"
        "  border-radius: 4px;"
        "  background: #1e1e1e;"
        "  color: #e0e0e0;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus { border: 1px solid #5a9; }");
    eventInputLayout->addWidget(m_eventNameInput);

    QPushButton* subscribeButton = new QPushButton(QStringLiteral("Subscribe"), this);
    subscribeButton->setObjectName(QStringLiteral("subscribeButton"));
    subscribeButton->setStyleSheet(kButtonStyle);
    connect(subscribeButton, &QPushButton::clicked, this, &MainWindow::onSubscribeEvent);
    eventInputLayout->addWidget(subscribeButton);

    layout->addLayout(eventInputLayout);

    m_eventLog = new QTextEdit(this);
    m_eventLog->setObjectName(QStringLiteral("eventLog"));
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
        "}");
    m_eventLog->setPlaceholderText(QStringLiteral("Event log will appear here..."));

    m_methodsTree = new QTreeWidget(this);
    m_methodsTree->setObjectName(QStringLiteral("methodsTree"));
    m_methodsTree->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Return Type"),
                                    QStringLiteral("Parameters")});
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
        "QTreeWidget::item { padding: 6px 8px; color: #e0e0e0; }"
        "QTreeWidget::item:selected { background-color: #3a5a7a; color: #ffffff; }"
        "QHeaderView::section {"
        "  background-color: #1a1a1a;"
        "  color: #e0e0e0;"
        "  padding: 10px 8px;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  border: none;"
        "  border-right: 1px solid #3d3d3d;"
        "}");

    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_eventLog);
    splitter->addWidget(m_methodsTree);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({200, 600});
    splitter->setStyleSheet(
        "QSplitter::handle { background-color: #3d3d3d; height: 4px; }"
        "QSplitter::handle:hover { background-color: #5a9; }");
    layout->addWidget(splitter);

    setCentralWidget(centralWidget);
}

void MainWindow::showHeaderError(const QString& html)
{
    m_headerLabel->setText(html);
    m_headerLabel->setStyleSheet(kHeaderErrorStyle);
}

QWidget* MainWindow::createMethodForm(const ModuleLib::MethodInfo& method)
{
    QWidget* formContainer = new QWidget();
    formContainer->setObjectName(QStringLiteral("methodFormContainer"));
    formContainer->setProperty("methodName", method.name);
    formContainer->setStyleSheet(QStringLiteral("background-color: #2a2a2a; border-radius: 4px;"));

    QVBoxLayout* mainLayout = new QVBoxLayout(formContainer);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    QFormLayout* formLayout = new QFormLayout();
    formLayout->setSpacing(8);
    formLayout->setLabelAlignment(Qt::AlignRight);

    for (int p = 0; p < int(method.parameters.size()); ++p) {
        const ModuleLib::ParameterInfo& param = method.parameters[size_t(p)];
        const QString paramType = param.type;
        QString paramName = param.name;
        if (paramName.isEmpty()) {
            paramName = QStringLiteral("param%1").arg(p);
        }

        const QString normalizedType = normalizeType(paramType);
        QWidget* inputWidget = nullptr;

        if (normalizedType == QLatin1String("int") || normalizedType == QLatin1String("long")
            || normalizedType == QLatin1String("int64_t") || normalizedType == QLatin1String("qint64")
            || normalizedType == QLatin1String("int32_t") || normalizedType == QLatin1String("qint32")) {
            QSpinBox* spin = new QSpinBox();
            spin->setRange(-2147483647, 2147483647);
            inputWidget = spin;
        } else if (normalizedType == QLatin1String("double") || normalizedType == QLatin1String("float")
                   || normalizedType == QLatin1String("qreal")) {
            QDoubleSpinBox* spin = new QDoubleSpinBox();
            spin->setRange(-1e10, 1e10);
            spin->setDecimals(6);
            inputWidget = spin;
        } else if (normalizedType == QLatin1String("bool")) {
            inputWidget = new QCheckBox();
        } else {
            QLineEdit* edit = new QLineEdit();
            edit->setPlaceholderText(paramType);
            inputWidget = edit;
        }

        inputWidget->setObjectName(QStringLiteral("param_%1").arg(p));
        inputWidget->setProperty("paramType", paramType);
        inputWidget->setStyleSheet(kInputStyle);

        const QString labelText =
            QStringLiteral("<b style='color: #e0e0e0;'>%1</b> "
                           "<span style='color: #888;'>(%2)</span>")
                .arg(paramName, paramType);
        QLabel* label = new QLabel(labelText);
        label->setStyleSheet(QStringLiteral("color: #e0e0e0;"));
        formLayout->addRow(label, inputWidget);
    }

    if (method.parameters.empty()) {
        QLabel* noParams = new QLabel(QStringLiteral("<i style='color: #888;'>No parameters</i>"));
        noParams->setStyleSheet(QStringLiteral("color: #888;"));
        formLayout->addRow(noParams);
    }

    mainLayout->addLayout(formLayout);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    // Per-method button text and objectName so UI tests (and humans) can target a
    // specific method's "Call" button unambiguously — every form has its own.
    QPushButton* callButton =
        new QPushButton(QStringLiteral("Call %1").arg(method.name));
    callButton->setObjectName(QStringLiteral("callButton_%1").arg(method.name));
    callButton->setProperty("methodName", method.name);
    callButton->setStyleSheet(kButtonStyle);
    connect(callButton, &QPushButton::clicked, this, &MainWindow::onCallMethod);

    buttonLayout->addWidget(callButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    QFrame* resultFrame = new QFrame();
    resultFrame->setObjectName(QStringLiteral("resultFrame"));
    resultFrame->setMinimumHeight(80);
    resultFrame->setStyleSheet(
        "QFrame {"
        "  background-color: #1e1e1e;"
        "  border: 1px solid #4d4d4d;"
        "  border-radius: 4px;"
        "  padding: 12px;"
        "}");
    QVBoxLayout* resultLayout = new QVBoxLayout(resultFrame);
    resultLayout->setContentsMargins(12, 12, 12, 12);
    resultLayout->setSpacing(8);

    QLabel* resultTitle = new QLabel(QStringLiteral("<b style='color: #e0e0e0;'>Result:</b>"));
    resultTitle->setStyleSheet(QStringLiteral("color: #e0e0e0;"));
    resultLayout->addWidget(resultTitle);

    QLabel* resultLabel = new QLabel(QStringLiteral("<i style='color: #888;'>Not called yet</i>"));
    resultLabel->setObjectName(QStringLiteral("resultLabel"));
    resultLabel->setWordWrap(true);
    resultLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    resultLabel->setStyleSheet(
        "QLabel {"
        "  padding: 8px;"
        "  background-color: #252525;"
        "  border-radius: 4px;"
        "  font-family: 'SF Mono', 'Menlo', 'Monaco', monospace;"
        "  font-size: 12px;"
        "}");
    resultLayout->addWidget(resultLabel);

    mainLayout->addWidget(resultFrame);

    return formContainer;
}

void MainWindow::onCallMethod()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;

    const QString methodName = button->property("methodName").toString();

    QWidget* formWidget = button->parentWidget();
    while (formWidget && formWidget->objectName() != QLatin1String("methodFormContainer")) {
        formWidget = formWidget->parentWidget();
    }
    if (!formWidget) return;

    invokeMethod(methodName, formWidget);
}

void MainWindow::invokeMethod(const QString& methodName, QWidget* formWidget)
{
    QLabel* resultLabel =
        formWidget->findChild<QLabel*>(QStringLiteral("resultLabel"), Qt::FindChildrenRecursively);

    // Resolve the method (for parameter ordering) from the core's introspection.
    const ModuleLib::MethodInfo* method = nullptr;
    for (const ModuleLib::MethodInfo& m : m_core->currentModule().methods) {
        if (m.name == methodName) {
            method = &m;
            break;
        }
    }
    if (!method) {
        if (resultLabel) {
            resultLabel->setText(QStringLiteral(
                "<span style='color: #ff6b6b;'><b>Error:</b> method not found</span>"));
        }
        return;
    }

    // Gather argument values from the form widgets, coercing via ViewerCore.
    QVariantList args;
    for (int p = 0; p < int(method->parameters.size()); ++p) {
        QWidget* inputWidget = formWidget->findChild<QWidget*>(
            QStringLiteral("param_%1").arg(p), Qt::FindChildrenRecursively);
        const QString paramType =
            inputWidget ? inputWidget->property("paramType").toString()
                        : method->parameters[size_t(p)].type;

        if (QSpinBox* spin = qobject_cast<QSpinBox*>(inputWidget)) {
            args << QVariant(qlonglong(spin->value()));
        } else if (QDoubleSpinBox* dspin = qobject_cast<QDoubleSpinBox*>(inputWidget)) {
            args << QVariant(dspin->value());
        } else if (QCheckBox* check = qobject_cast<QCheckBox*>(inputWidget)) {
            args << QVariant(check->isChecked());
        } else if (QLineEdit* edit = qobject_cast<QLineEdit*>(inputWidget)) {
            args << ViewerCore::coerceArg(paramType, edit->text());
        } else {
            args << QVariant(QString());
        }
    }

    if (resultLabel) {
        resultLabel->setText(QStringLiteral("<i style='color: #888;'>Calling remote method...</i>"));
        QCoreApplication::processEvents();
    }

    const ViewerCore::CallResult result = m_core->callMethod(methodName, args);

    if (!resultLabel) return;

    if (!result.ok) {
        resultLabel->setText(
            QStringLiteral("<span style='color: #ff6b6b;'><b>Error:</b> %1</span>")
                .arg(result.error.toHtmlEscaped()));
        return;
    }

    if (result.isVoid) {
        resultLabel->setText(QStringLiteral(
            "<span style='color: #5a9;'>Method called successfully (void return)</span>"));
        return;
    }

    QString resultText = result.value.toString();
    if (resultText.isEmpty() && result.value.canConvert<QStringList>()) {
        resultText = result.value.toStringList().join(QStringLiteral(", "));
    }
    if (resultText.isEmpty() && result.value.isValid()) {
        resultText = QStringLiteral("(%1)").arg(QString::fromUtf8(result.value.typeName()));
    }
    if (resultText.isEmpty()) {
        resultText = QStringLiteral("(empty or null result)");
    }

    resultLabel->setText(
        QStringLiteral("<span style='color: #5a9;'><b>Result:</b></span> "
                       "<span style='color: #e0e0e0;'>%1</span>")
            .arg(resultText.toHtmlEscaped()));
}

void MainWindow::onSubscribeEvent()
{
    if (!m_eventNameInput) return;

    const QString eventName = m_eventNameInput->text().trimmed();
    if (eventName.isEmpty()) {
        appendEventToLog(QStringLiteral("Error"),
                         QVariantList() << QStringLiteral("Event name cannot be empty"));
        return;
    }
    if (m_eventSubscriptions.contains(eventName)) {
        appendEventToLog(QStringLiteral("Warning"),
                         QVariantList() << QStringLiteral("Already subscribed to event: %1").arg(eventName));
        return;
    }
    if (!m_core->hasModule()) {
        appendEventToLog(QStringLiteral("Error"),
                         QVariantList() << QStringLiteral("No module loaded"));
        return;
    }

    QString error;
    const bool ok = m_core->subscribeEvent(
        eventName,
        [this](const QString& name, const QVariantList& data) {
            appendEventToLog(name, data);
        },
        &error);

    if (!ok) {
        appendEventToLog(QStringLiteral("Error"), QVariantList() << error);
        return;
    }

    m_eventSubscriptions.insert(eventName);
    appendEventToLog(QStringLiteral("Info"),
                     QVariantList() << QStringLiteral("Subscribed to event: %1").arg(eventName));
    m_eventNameInput->clear();
}

void MainWindow::appendEventToLog(const QString& eventName, const QVariantList& data)
{
    if (!m_eventLog) return;

    QJsonObject eventObj;
    eventObj["event"] = eventName;
    eventObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray dataArray;
    for (const QVariant& v : data) {
        dataArray.append(QJsonValue::fromVariant(v));
    }
    eventObj["data"] = dataArray;

    const QString jsonString = QJsonDocument(eventObj).toJson(QJsonDocument::Indented);
    m_eventLog->append(jsonString);

    QTextCursor cursor = m_eventLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_eventLog->setTextCursor(cursor);
}

void MainWindow::loadModule(const QString& path)
{
    m_methodsTree->clear();
    m_eventSubscriptions.clear();
    if (m_eventLog) {
        m_eventLog->clear();
    }

    const ViewerCore::ModuleInfo info = m_core->loadModule(path);

    if (!info.ok) {
        showHeaderError(
            QStringLiteral("<b style='color: #ff6b6b;'>Error:</b> %1")
                .arg(info.error.toHtmlEscaped()));
        return;
    }

    // Header.
    QString headerText =
        QStringLiteral("<b style='font-size: 16px; color: #e0e0e0;'>%1</b>").arg(info.name);
    if (!info.version.isEmpty()) {
        headerText += QStringLiteral(" <span style='color: #888; font-size: 13px;'>v%1</span>")
                          .arg(info.version);
    }
    headerText += QStringLiteral("<br><span style='color: #888; font-size: 12px;'>%1</span>")
                      .arg(info.path);
    headerText += QStringLiteral("<br><span style='color: #5a9; font-size: 11px;'>Remote module: %1</span>")
                      .arg(info.name);
    m_headerLabel->setText(headerText);
    m_headerLabel->setStyleSheet(kHeaderOkStyle);

    // Methods tree: each method is a top-level row with an expandable call form.
    for (const ModuleLib::MethodInfo& method : info.methods) {
        QStringList paramStrings;
        for (const ModuleLib::ParameterInfo& p : method.parameters) {
            QString pname = p.name.isEmpty() ? QStringLiteral("arg") : p.name;
            paramStrings << QStringLiteral("%1 %2").arg(p.type, pname);
        }
        QString parameters = paramStrings.join(QStringLiteral(", "));
        if (parameters.isEmpty()) {
            parameters = QStringLiteral("(none)");
        }
        QString returnType = method.returnType.isEmpty() ? QStringLiteral("void")
                                                         : method.returnType;

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, method.name);
        item->setText(1, returnType);
        item->setText(2, parameters);
        item->setForeground(1, QColor("#5a9"));
        QFont nameFont = item->font(0);
        nameFont.setBold(true);
        item->setFont(0, nameFont);
        m_methodsTree->addTopLevelItem(item);

        if (method.name != QLatin1String("initLogos")) {
            QTreeWidgetItem* formItem = new QTreeWidgetItem(item);
            formItem->setFirstColumnSpanned(true);
            m_methodsTree->setItemWidget(formItem, 0, createMethodForm(method));
        } else {
            item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
        }
    }

    m_methodsTree->expandAll();
    setWindowTitle(QStringLiteral("Logos Module Viewer - %1").arg(info.name));
}
