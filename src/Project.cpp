#include "Project.h"

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
    return joinPath(rootPath, "forms" + QString("/%1.form.json").arg(form.name));
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
    root.insert("version", 1);
    root.insert("name", name);
    root.insert("vars", vars);

    QJsonArray formPaths;
    for (const FormModel &form : forms) {
        if (!writeJson(formFilePath(form), form.toJson(), error)) {
            return false;
        }
        formPaths.append(QString("forms/%1.form.json").arg(form.name));
    }
    root.insert("forms", formPaths);

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

    const QJsonArray formPaths = root.value("forms").toArray();
    for (const QJsonValue &value : formPaths) {
        const QString path = joinPath(project.rootPath, value.toString());
        QFile formFile(path);
        if (!formFile.open(QIODevice::ReadOnly)) {
            if (error) {
                *error = QString("Não foi possível abrir formulário %1.").arg(path);
            }
            return project;
        }
        const QJsonDocument formDocument = QJsonDocument::fromJson(formFile.readAll());
        QString formError;
        FormModel form = FormModel::fromJson(formDocument.object(), &formError);
        if (!formError.isEmpty()) {
            if (error) {
                *error = formError;
            }
            return project;
        }
        project.forms.append(form);
    }
    return project;
}
