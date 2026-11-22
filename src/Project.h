#pragma once

#include "FormModel.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

struct Project {
    QString name;
    QString rootPath;
    QVector<FormModel> forms;
    QJsonObject vars;

    static Project create(const QString &rootPath, const QString &name);
    static Project load(const QString &projectFilePath, QString *error = nullptr);

    bool save(QString *error = nullptr) const;

    QString projectFilePath() const;
    QString formFilePath(const FormModel &form) const;

private:
    static bool writeJson(const QString &path, const QJsonObject &object, QString *error);
};
