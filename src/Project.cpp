#include "Project.h"

#include "UiForm.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {
QString joinPath(const QString &root, const QString &relative)
{
    return QDir::cleanPath(root + QLatin1Char('/') + relative);
}

void collectBindings(const WidgetNode &node, QJsonObject &target)
{
    QJsonObject properties;
    for (auto it = node.properties.constBegin(); it != node.properties.constEnd(); ++it) {
        if (it.value().isObject()) {
            const QJsonObject binding = it.value().toObject();
            if (binding.contains(QStringLiteral("$var"))) {
                properties.insert(it.key(), binding.value(QStringLiteral("$var")));
            }
        }
    }
    if (!properties.isEmpty()) {
        target.insert(node.objectName, properties);
    }
    for (const WidgetNode &child : node.children) {
        collectBindings(child, target);
    }
}

void applyBindings(WidgetNode &node, const QJsonObject &bindings)
{
    const QJsonObject properties = bindings.value(node.objectName).toObject();
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        QJsonObject binding;
        binding.insert(QStringLiteral("$var"), it.value());
        node.properties.insert(it.key(), binding);
    }
    for (WidgetNode &child : node.children) {
        applyBindings(child, bindings);
    }
}

void collectEvents(const WidgetNode &node, QJsonObject &target)
{
    QJsonArray events;
    for (const EventSpec &event : node.events) {
        QJsonObject object;
        object.insert(QStringLiteral("signal"), event.signal);
        object.insert(QStringLiteral("slot"), event.slot);
        events.append(object);
    }
    if (!events.isEmpty()) {
        target.insert(node.objectName, events);
    }
    for (const WidgetNode &child : node.children) {
        collectEvents(child, target);
    }
}

void applyEvents(WidgetNode &node, const QJsonObject &events)
{
    const QJsonArray list = events.value(node.objectName).toArray();
    for (const QJsonValue &value : list) {
        const QJsonObject object = value.toObject();
        const EventSpec event{object.value(QStringLiteral("signal")).toString(),
                              object.value(QStringLiteral("slot")).toString()};
        if (event.signal.isEmpty() || event.slot.isEmpty()) {
            continue;
        }
        bool exists = false;
        for (const EventSpec &current : node.events) {
            if (current.signal == event.signal && current.slot == event.slot) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            node.events.append(event);
        }
    }
    for (WidgetNode &child : node.children) {
        applyEvents(child, events);
    }
}

bool writeText(const QString &path, const QString &text, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("Não foi possível escrever %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    file.write(text.toUtf8());
    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("Não foi possível salvar %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    return true;
}
} // namespace

Project Project::create(const QString &rootPath, const QString &name)
{
    Project project;
    project.name = name.isEmpty() ? "MeuProjeto" : name;
    project.rootPath = QDir::cleanPath(rootPath);

    project.vars.insert("corPrimaria", "#2d7d9a");
    project.vars.insert("alturaBotao", 32);
    project.vars.insert("titulo", project.name);

    project.forms.append(FormModel::defaultForm());
    return project;
}

QString Project::projectFilePath() const
{
    return joinPath(rootPath, "project.json");
}

QString Project::formFilePath(const FormModel &form) const
{
    return joinPath(rootPath, QStringLiteral("forms/%1.ui").arg(form.name));
}

bool Project::writeJson(const QString &path, const QJsonObject &object, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QString("Não foi possível escrever %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) {
            *error = QString("Não foi possível salvar %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    return true;
}

bool Project::save(QString *error) const
{
    QDir().mkpath(joinPath(rootPath, "forms"));
    QDir().mkpath(joinPath(rootPath, "generated"));
    QDir().mkpath(joinPath(rootPath, "src"));

    QJsonObject root;
    root.insert("version", 2);
    root.insert("name", name);
    root.insert("vars", vars);

    QJsonArray formPaths;
    QJsonObject bindings;
    QJsonObject events;
    for (const FormModel &form : forms) {
        if (!writeText(formFilePath(form), UiForm::toUi(form), error)) {
            return false;
        }
        QJsonObject formBindings;
        collectBindings(form.root, formBindings);
        bindings.insert(form.name, formBindings);
        QJsonObject formEvents;
        collectEvents(form.root, formEvents);
        events.insert(form.name, formEvents);
        formPaths.append(QStringLiteral("forms/%1.ui").arg(form.name));
    }
    root.insert("forms", formPaths);
    root.insert("bindings", bindings);
    root.insert("events", events);

    return writeJson(projectFilePath(), root, error);
}

Project Project::load(const QString &projectFilePath, QString *error)
{
    Project project;
    QFile file(projectFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QString("Não foi possível abrir %1: %2").arg(projectFilePath, file.errorString());
        }
        return project;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        if (error) {
            *error = "project.json inválido: esperado um objeto JSON.";
        }
        return project;
    }

    const QJsonObject root = document.object();
    project.name = root.value("name").toString("MeuProjeto");
    project.rootPath = QFileInfo(projectFilePath).absolutePath();
    project.vars = root.value("vars").toObject();

    const QJsonObject events = root.value("events").toObject();
    const QJsonArray formPaths = root.value("forms").toArray();
    for (const QJsonValue &value : formPaths) {
        const QString path = joinPath(project.rootPath, value.toString());
        QFile formFile(path);
        if (!formFile.open(QIODevice::ReadOnly)) {
            if (error) {
                *error = QStringLiteral("Não foi possível abrir formulário %1.").arg(path);
            }
            return project;
        }
        const QByteArray content = formFile.readAll();
        QString formError;
        FormModel form;
        const bool isUi = path.endsWith(QStringLiteral(".ui"));
        if (isUi) {
            // O .ui é a fonte autoritativa das conexões (o uic gera os connects).
            form = UiForm::fromUi(QString::fromUtf8(content), &formError);
        } else {
            form = FormModel::fromJson(QJsonDocument::fromJson(content).object(), &formError);
            applyEvents(form.root, events.value(form.name).toObject());
        }
        if (!formError.isEmpty()) {
            if (error) {
                *error = formError;
            }
            return project;
        }
        project.forms.append(form);
    }

    const QJsonObject bindings = root.value("bindings").toObject();
    for (FormModel &form : project.forms) {
        applyBindings(form.root, bindings.value(form.name).toObject());
    }
    return project;
}
