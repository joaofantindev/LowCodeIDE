#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

struct EventSpec {
    QString signal;
    QString slot;
};

struct WidgetNode {
    QString type;
    QString objectName;
    QString layout;
    QJsonObject properties;
    QVector<EventSpec> events;
    QVector<WidgetNode> children;
};

struct FormModel {
    QString name = "MainWindow";
    QString baseClass = "QWidget";
    WidgetNode root;

    static FormModel defaultForm();
    static FormModel blankForm(const QString &name);
    static FormModel fromJson(const QJsonObject &object, QString *error = nullptr);
    QJsonObject toJson() const;

private:
    static WidgetNode nodeFromJson(const QJsonObject &object);
    static QJsonObject nodeToJson(const WidgetNode &node);
};
