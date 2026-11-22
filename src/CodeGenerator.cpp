#include "CodeGenerator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSet>
#include <QTextStream>

namespace {

QString cppString(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    escaped.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    return QStringLiteral("QStringLiteral(\"%1\")").arg(escaped);
}

QString valueExpr(const QJsonValue &value)
{
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'g', 15);
    }
    return cppString(value.toString());
}

QString setterFor(const QString &property)
{
    static const QHash<QString, QString> setters{
        {QStringLiteral("text"), QStringLiteral("setText")},
        {QStringLiteral("title"), QStringLiteral("setTitle")},
        {QStringLiteral("placeholderText"), QStringLiteral("setPlaceholderText")},
        {QStringLiteral("toolTip"), QStringLiteral("setToolTip")},
        {QStringLiteral("statusTip"), QStringLiteral("setStatusTip")},
        {QStringLiteral("windowTitle"), QStringLiteral("setWindowTitle")},
        {QStringLiteral("enabled"), QStringLiteral("setEnabled")},
        {QStringLiteral("visible"), QStringLiteral("setVisible")},
        {QStringLiteral("minimumWidth"), QStringLiteral("setMinimumWidth")},
        {QStringLiteral("minimumHeight"), QStringLiteral("setMinimumHeight")},
        {QStringLiteral("maximumWidth"), QStringLiteral("setMaximumWidth")},
        {QStringLiteral("maximumHeight"), QStringLiteral("setMaximumHeight")},
        {QStringLiteral("fixedWidth"), QStringLiteral("setFixedWidth")},
        {QStringLiteral("fixedHeight"), QStringLiteral("setFixedHeight")},
        {QStringLiteral("styleSheet"), QStringLiteral("setStyleSheet")},
        {QStringLiteral("alignment"), QStringLiteral("setAlignment")},
        {QStringLiteral("checkable"), QStringLiteral("setCheckable")},
        {QStringLiteral("checked"), QStringLiteral("setChecked")},
        {QStringLiteral("readOnly"), QStringLiteral("setReadOnly")},
    };
    return setters.value(property);
}

struct ConnectionInfo {
    QString sender;
    QString type;
    QString signal;
    QString slot;
};

struct GenContext {
    QStringList declarations;
    QStringList setupLines;
    QStringList bindLines;
    QSet<QString> includes{QStringLiteral("QWidget")};
    QSet<QString> senderTypes;
    QSet<QString> slotNames;
    QVector<ConnectionInfo> connections;
};

void genNode(const WidgetNode &node, const QString &parentExpr,
             const QString &parentLayoutExpr, bool isRoot, GenContext &ctx)
{
    const QString type = node.type.isEmpty() ? QStringLiteral("QWidget") : node.type;
    ctx.includes.insert(type);

    const bool needsLayout = !node.layout.isEmpty() || !node.children.isEmpty();
    const QString layoutType = node.layout.isEmpty() ? QStringLiteral("QVBoxLayout") : node.layout;
    QString layoutVar;
    if (needsLayout) {
        ctx.includes.insert(layoutType);
        layoutVar = isRoot ? QStringLiteral("rootLayout") : node.objectName + QStringLiteral("Layout");
        ctx.declarations << QStringLiteral("%1* %2 = nullptr;").arg(layoutType, layoutVar);
    }

    const QString target = isRoot ? QStringLiteral("host") : node.objectName;
    if (isRoot) {
        ctx.setupLines << QStringLiteral("host = h;");
    } else {
        ctx.declarations << QStringLiteral("%1* %2 = nullptr;").arg(type, node.objectName);
        ctx.setupLines << QStringLiteral("%1 = new %2(%3);").arg(node.objectName, type, parentExpr);
        ctx.setupLines << QStringLiteral("%1->setObjectName(QStringLiteral(\"%2\"));")
                              .arg(node.objectName, node.objectName);
    }

    if (!layoutVar.isEmpty()) {
        ctx.setupLines << QStringLiteral("%1 = new %2(%3);")
                              .arg(layoutVar, layoutType, isRoot ? QStringLiteral("h") : target);
    }

    for (auto it = node.properties.constBegin(); it != node.properties.constEnd(); ++it) {
        const QString property = it.key();
        if (property == QStringLiteral("objectName")) {
            continue;
        }
        const QString setter = setterFor(property);
        if (setter.isEmpty()) {
            continue;
        }

        QString expression;
        bool bound = false;
        if (it.value().isObject() && it.value().toObject().contains(QStringLiteral("$var"))) {
            expression = QStringLiteral("vars().%1")
                             .arg(it.value().toObject().value(QStringLiteral("$var")).toString());
            bound = true;
        } else if (property == QStringLiteral("alignment") && it.value().isString()) {
            const QString value = it.value().toString();
            expression = value.startsWith(QStringLiteral("Qt::")) ? value : QStringLiteral("Qt::") + value;
        } else {
            expression = valueExpr(it.value());
        }

        const QString line = QStringLiteral("%1->%2(%3);").arg(target, setter, expression);
        ctx.setupLines << line;
        if (bound) {
            ctx.bindLines << line;
        }
    }

    if (!isRoot && !parentLayoutExpr.isEmpty()) {
        ctx.setupLines << QStringLiteral("%1->addWidget(%2);").arg(parentLayoutExpr, node.objectName);
    }

    for (const EventSpec &event : node.events) {
        if (event.signal.isEmpty() || event.slot.isEmpty()) {
            continue;
        }
        ctx.connections.append({node.objectName, type, event.signal, event.slot});
        ctx.senderTypes.insert(type);
        ctx.slotNames.insert(event.slot);
    }

    for (const WidgetNode &child : node.children) {
        genNode(child, target, layoutVar, false, ctx);
    }
}

QString generateUiHeader(const FormModel &form, const GenContext &ctx)
{
    QStringList lines;
    lines << QStringLiteral("#pragma once") << QStringLiteral("")
          << QStringLiteral("#include <QWidget>");
    QStringList includes = ctx.includes.values();
    includes.removeAll(QStringLiteral("QWidget"));
    includes.sort();
    for (const QString &include : includes) {
        lines << QStringLiteral("#include <%1>").arg(include);
    }
    lines << QStringLiteral("#include \"app_vars.h\"") << QStringLiteral("")
          << QStringLiteral("namespace Ui {")
          << QStringLiteral("class %1 {").arg(form.name)
          << QStringLiteral("public:")
          << QStringLiteral("    QWidget* host = nullptr;");
    for (const QString &declaration : ctx.declarations) {
        lines << QStringLiteral("    ") + declaration;
    }
    lines << QStringLiteral("")
          << QStringLiteral("    void setupUi(QWidget* h) {");
    for (const QString &line : ctx.setupLines) {
        lines << QStringLiteral("        ") + line;
    }
    lines << QStringLiteral("    }") << QStringLiteral("")
          << QStringLiteral("    void applyBindings() {");
    for (const QString &line : ctx.bindLines) {
        lines << QStringLiteral("        ") + line;
    }
    lines << QStringLiteral("    }") << QStringLiteral("};")
          << QStringLiteral("} // namespace Ui") << QStringLiteral("");
    return lines.join(QLatin1Char('\n'));
}

QString varType(const QJsonValue &value)
{
    if (value.isBool()) {
        return QStringLiteral("bool");
    }
    if (value.isDouble()) {
        const QString number = QString::number(value.toDouble(), 'g', 15);
        return number.contains(QLatin1Char('.')) ? QStringLiteral("double") : QStringLiteral("int");
    }
    return QStringLiteral("QString");
}

QString generateVarsHeader(const Project &project)
{
    QStringList lines;
    lines << QStringLiteral("#pragma once") << QStringLiteral("")
          << QStringLiteral("#include <QString>") << QStringLiteral("")
          << QStringLiteral("struct AppVars {");
    for (auto it = project.vars.constBegin(); it != project.vars.constEnd(); ++it) {
        lines << QStringLiteral("    %1 %2{%3};").arg(varType(it.value()), it.key(), valueExpr(it.value()));
    }
    lines << QStringLiteral("")
          << QStringLiteral("    void load(const QString& path);")
          << QStringLiteral("    void save(const QString& path) const;")
          << QStringLiteral("};")
          << QStringLiteral("")
          << QStringLiteral("AppVars& vars();")
          << QStringLiteral("");
    return lines.join(QLatin1Char('\n'));
}

QString generateVarsSource(const Project &project)
{
    QStringList lines;
    lines << QStringLiteral("#include \"app_vars.h\"") << QStringLiteral("")
          << QStringLiteral("#include <QFile>")
          << QStringLiteral("#include <QJsonDocument>")
          << QStringLiteral("#include <QJsonObject>")
          << QStringLiteral("")
          << QStringLiteral("AppVars& vars()")
          << QStringLiteral("{")
          << QStringLiteral("    static AppVars value;")
          << QStringLiteral("    return value;")
          << QStringLiteral("}")
          << QStringLiteral("")
          << QStringLiteral("void AppVars::load(const QString& path)")
          << QStringLiteral("{")
          << QStringLiteral("    QFile file(path);")
          << QStringLiteral("    if (!file.open(QIODevice::ReadOnly)) {")
          << QStringLiteral("        return;")
          << QStringLiteral("    }")
          << QStringLiteral("    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());")
          << QStringLiteral("    if (!document.isObject()) {")
          << QStringLiteral("        return;")
          << QStringLiteral("    }")
          << QStringLiteral("    const QJsonObject object = document.object();");
    for (auto it = project.vars.constBegin(); it != project.vars.constEnd(); ++it) {
        const QString key = it.key();
        const QString type = varType(it.value());
        const QString check = type == QStringLiteral("QString")
                                  ? QStringLiteral("isString()")
                                  : (type == QStringLiteral("bool") ? QStringLiteral("isBool()")
                                                                    : QStringLiteral("isDouble()"));
        const QString cast = type == QStringLiteral("QString")
                                 ? QStringLiteral("toString()")
                                 : (type == QStringLiteral("bool") ? QStringLiteral("toBool()")
                                                                   : QStringLiteral("toInt()"));
        lines << QStringLiteral("    if (object.contains(\"%1\") && object.value(\"%1\").%2) {")
                     .arg(key, check);
        lines << QStringLiteral("        %1 = object.value(\"%2\").%3;").arg(key, key, cast);
        lines << QStringLiteral("    }");
    }
    lines << QStringLiteral("}")
          << QStringLiteral("")
          << QStringLiteral("void AppVars::save(const QString& path) const")
          << QStringLiteral("{")
          << QStringLiteral("    QJsonObject object;");
    for (auto it = project.vars.constBegin(); it != project.vars.constEnd(); ++it) {
        lines << QStringLiteral("    object.insert(\"%1\", %1);").arg(it.key());
    }
    lines << QStringLiteral("    QFile file(path);")
          << QStringLiteral("    if (file.open(QIODevice::WriteOnly)) {")
          << QStringLiteral("        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));")
          << QStringLiteral("    }")
          << QStringLiteral("}")
          << QStringLiteral("");
    return lines.join(QLatin1Char('\n'));
}

QString generateMainSource(const QString &formName)
{
    return QStringLiteral(R"cpp(#include <QApplication>

#include "app_vars.h"
#include "%1.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    vars().load(QCoreApplication::applicationDirPath() + QStringLiteral("/app.vars.json"));

    %1 window;
    window.show();
    return app.exec();
}
)cpp")
        .arg(formName);
}

QString generateFormHeader(const QString &formName, const GenContext &ctx)
{
    QStringList lines;
    lines << QStringLiteral("#pragma once") << QStringLiteral("")
          << QStringLiteral("#include <QWidget>")
          << QStringLiteral("#include \"ui_%1.h\"").arg(formName.toLower())
          << QStringLiteral("")
          << QStringLiteral("class %1 : public QWidget").arg(formName)
          << QStringLiteral("{")
          << QStringLiteral("    Q_OBJECT")
          << QStringLiteral("public:")
          << QStringLiteral("    explicit %1(QWidget* parent = nullptr);").arg(formName);
    if (!ctx.slotNames.isEmpty()) {
        lines << QStringLiteral("")
              << QStringLiteral("private slots:");
        QStringList slotList = ctx.slotNames.values();
        slotList.sort();
        for (const QString &slot : slotList) {
            lines << QStringLiteral("    void %1();").arg(slot);
        }
    }
    lines << QStringLiteral("")
          << QStringLiteral("private:")
          << QStringLiteral("    Ui::%1 ui;").arg(formName)
          << QStringLiteral("};")
          << QStringLiteral("");
    return lines.join(QLatin1Char('\n'));
}

QString generateFormSource(const QString &formName, const GenContext &ctx)
{
    QStringList lines;
    lines << QStringLiteral("#include \"%1.h\"").arg(formName) << QStringLiteral("")
          << QStringLiteral("#include <QMessageBox>");
    QStringList senderTypes = ctx.senderTypes.values();
    senderTypes.sort();
    for (const QString &type : senderTypes) {
        lines << QStringLiteral("#include <%1>").arg(type);
    }
    lines << QStringLiteral("")
          << QStringLiteral("%1::%1(QWidget* parent)").arg(formName)
          << QStringLiteral("    : QWidget(parent)")
          << QStringLiteral("{")
          << QStringLiteral("    ui.setupUi(this);")
          << QStringLiteral("    // >> IDE: conexões geradas (não edite esta seção)");

    for (const ConnectionInfo &connection : ctx.connections) {
        lines << QStringLiteral("    connect(ui.%1, &%2::%3, this, &%4::%5);")
                     .arg(connection.sender, connection.type, connection.signal, formName,
                          connection.slot);
    }
    lines << QStringLiteral("    // << IDE: conexões geradas")
          << QStringLiteral("}");

    QStringList slotList = ctx.slotNames.values();
    slotList.sort();
    for (const QString &slot : slotList) {
        lines << QStringLiteral("")
              << QStringLiteral("void %1::%2()").arg(formName, slot)
              << QStringLiteral("{")
              << QStringLiteral("    // TODO: escreva aqui a lógica deste evento.")
              << QStringLiteral("    QMessageBox::information(this, QStringLiteral(\"%1\"),").arg(formName)
              << QStringLiteral("                            QStringLiteral(\"Evento gerado pela IDE.\"));")
              << QStringLiteral("}");
    }
    lines << QStringLiteral("");
    return lines.join(QLatin1Char('\n'));
}

QString generateDeployScript()
{
    return QStringLiteral(R"PS1(param(
    [Parameter(Mandatory = $true)]
    [string] $Executable
)

$ErrorActionPreference = 'Stop'

$exePath = (Resolve-Path -LiteralPath $Executable).Path
$destination = Split-Path -Parent $exePath
$bash = 'C:\msys64\usr\bin\bash.exe'
$mingwBin = 'C:\msys64\mingw64\bin'

if (-not (Test-Path -LiteralPath $bash)) {
    throw "MSYS2 não encontrado em C:\msys64."
}
if (-not (Test-Path -LiteralPath $mingwBin)) {
    throw "Pasta $mingwBin não encontrada."
}

$unixPath = '/c' + $exePath.Substring(2).Replace('\', '/')
$command = 'export PATH=/mingw64/bin:$PATH; ldd ' + "'$unixPath'"
$previousPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$lddOutput = & $bash -lc $command 2>$null
$ErrorActionPreference = $previousPreference

$dllNames = @()
foreach ($line in $lddOutput) {
    if ($line -match '/mingw64/bin/([^ ]+\.dll)') {
        $dllNames += $Matches[1]
    }
}
$dllNames = $dllNames | Sort-Object -Unique

foreach ($name in $dllNames) {
    $source = Join-Path $mingwBin $name
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $destination $name) -Force
    }
}

Write-Host "Dependências MinGW copiadas: $($dllNames.Count) DLL(s)."
)PS1");
}

QString generateCMake(const Project &project)
{
    const QString target = project.name.trimmed().replace(QLatin1Char(' '), QLatin1Char('_'));
    QStringList lines;
    lines << QStringLiteral("cmake_minimum_required(VERSION 3.25)")
          << QStringLiteral("")
          << QStringLiteral("project(%1 LANGUAGES CXX)").arg(target)
          << QStringLiteral("")
          << QStringLiteral("set(CMAKE_CXX_STANDARD 20)")
          << QStringLiteral("set(CMAKE_CXX_STANDARD_REQUIRED ON)")
          << QStringLiteral("set(CMAKE_AUTOMOC ON)")
          << QStringLiteral("set(CMAKE_EXPORT_COMPILE_COMMANDS ON)")
          << QStringLiteral("")
          << QStringLiteral("find_package(Qt6 REQUIRED COMPONENTS Widgets)")
          << QStringLiteral("")
          << QStringLiteral("qt_add_executable(%1").arg(target)
          << QStringLiteral("    src/main.cpp")
          << QStringLiteral("    src/MainWindow.cpp")
          << QStringLiteral("    src/MainWindow.h")
          << QStringLiteral("    generated/ui_mainwindow.h")
          << QStringLiteral("    generated/app_vars.cpp")
          << QStringLiteral("    generated/app_vars.h")
          << QStringLiteral(")")
          << QStringLiteral("")
          << QStringLiteral("target_include_directories(%1 PRIVATE src generated)").arg(target)
          << QStringLiteral("target_link_libraries(%1 PRIVATE Qt6::Widgets)").arg(target)
          << QStringLiteral("")
          << QStringLiteral("add_custom_command(TARGET %1 POST_BUILD").arg(target)
          << QStringLiteral("    COMMAND ${CMAKE_COMMAND} -E copy_if_different")
          << QStringLiteral("            ${CMAKE_CURRENT_SOURCE_DIR}/app.vars.json")
          << QStringLiteral("            $<TARGET_FILE_DIR:%1>/app.vars.json)").arg(target)
          << QStringLiteral("")
          << QStringLiteral("if(WIN32)")
          << QStringLiteral("    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt6 windeployqt HINTS \"C:/msys64/mingw64/bin\")")
          << QStringLiteral("    find_program(POWERSHELL_EXECUTABLE NAMES powershell.exe pwsh.exe)")
          << QStringLiteral("    if(WINDEPLOYQT_EXECUTABLE AND POWERSHELL_EXECUTABLE)")
          << QStringLiteral("        add_custom_command(TARGET %1 POST_BUILD").arg(target)
          << QStringLiteral("            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-translations --no-opengl-sw --no-system-d3d-compiler \"$<TARGET_FILE:%1>\"").arg(target)
          << QStringLiteral("            COMMAND \"${POWERSHELL_EXECUTABLE}\" -NoProfile -ExecutionPolicy Bypass")
          << QStringLiteral("                    -File \"${CMAKE_CURRENT_SOURCE_DIR}/tools/deploy-runtime.ps1\"")
          << QStringLiteral("                    \"$<TARGET_FILE:%1>\"").arg(target)
          << QStringLiteral("            COMMENT \"Copiando DLLs do Qt e dependências do MinGW...\"")
          << QStringLiteral("            VERBATIM)")
          << QStringLiteral("    endif()")
          << QStringLiteral("endif()")
          << QStringLiteral("");
    return lines.join(QLatin1Char('\n'));
}

bool writeGeneratedFile(const QString &path, const QString &content,
                        GenerateResult &result, QString *error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) {
            *error = QStringLiteral("Não foi possível criar a pasta de %1.").arg(path);
        }
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Não foi possível escrever %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    QTextStream stream(&file);
    stream << content;
    stream.flush();
    if (file.error() != QFile::NoError) {
        if (error) {
            *error = QStringLiteral("Erro ao gravar %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    result.writtenFiles << path;
    return true;
}

} // namespace

GenerateResult CodeGenerator::generateAll(const Project &project, bool overwriteUserFiles)
{
    GenerateResult result;
    if (project.forms.isEmpty()) {
        result.error = QStringLiteral("O projeto não possui formulários.");
        return result;
    }

    const QDir root(project.rootPath);
    const FormModel &form = project.forms.first();
    if (form.name.isEmpty()) {
        result.error = QStringLiteral("Formulário sem nome.");
        return result;
    }

    GenContext ctx;
    genNode(form.root, QStringLiteral("host"), QString(), true, ctx);

    auto write = [&](const QString &relative, const QString &content) {
        return writeGeneratedFile(root.filePath(relative), content, result, &result.error);
    };

    if (!write(QStringLiteral("generated/ui_%1.h").arg(form.name.toLower()),
               generateUiHeader(form, ctx))
        || !write(QStringLiteral("generated/app_vars.h"), generateVarsHeader(project))
        || !write(QStringLiteral("generated/app_vars.cpp"), generateVarsSource(project))
        || !write(QStringLiteral("tools/deploy-runtime.ps1"), generateDeployScript())
        || !write(QStringLiteral("CMakeLists.txt"), generateCMake(project))) {
        return result;
    }

    if (overwriteUserFiles || !root.exists(QStringLiteral("src/main.cpp"))) {
        if (!write(QStringLiteral("src/main.cpp"), generateMainSource(form.name))) {
            return result;
        }
    }
    const QString formHeader = QStringLiteral("src/%1.h").arg(form.name);
    const QString formSource = QStringLiteral("src/%1.cpp").arg(form.name);
    if (overwriteUserFiles || !root.exists(formHeader)) {
        if (!write(formHeader, generateFormHeader(form.name, ctx))) {
            return result;
        }
    }
    if (overwriteUserFiles || !root.exists(formSource)) {
        if (!write(formSource, generateFormSource(form.name, ctx))) {
            return result;
        }
    }

    QJsonObject varsJson;
    for (auto it = project.vars.constBegin(); it != project.vars.constEnd(); ++it) {
        varsJson.insert(it.key(), it.value());
    }
    if (!writeGeneratedFile(root.filePath(QStringLiteral("app.vars.json")),
                            QString::fromUtf8(QJsonDocument(varsJson).toJson(QJsonDocument::Indented)),
                            result, &result.error)) {
        return result;
    }

    result.ok = true;
    return result;
}
