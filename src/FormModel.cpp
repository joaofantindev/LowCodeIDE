#include "FormModel.h"

#include <QJsonArray>

namespace {
QJsonArray eventsToJson(const QVector<EventSpec> &events)
{
    QJsonArray array;
    for (const EventSpec &event : events) {
        QJsonObject object;
        object.insert("signal", event.signal);
        object.insert("slot", event.slot);
        array.append(object);
    }
    return array;
}

QVector<EventSpec> eventsFromJson(const QJsonValue &value)
{
    QVector<EventSpec> events;
    if (!value.isArray()) {
        return events;
    }
    const QJsonArray array = value.toArray();
    for (const QJsonValue &item : array) {
        if (!item.isObject()) {
            continue;
        }
        const QJsonObject object = item.toObject();
        events.append({object.value("signal").toString(), object.value("slot").toString()});
    }
    return events;
}
} // namespace

FormModel FormModel::defaultForm()
{
    FormModel form;
    form.name = "MainWindow";
    form.baseClass = "QWidget";

    form.root.type = "QWidget";
    form.root.objectName = "MainWindow";
    form.root.layout = "QVBoxLayout";
    QJsonObject titleBinding;
    titleBinding.insert("$var", "titulo");
    form.root.properties.insert("windowTitle", titleBinding);

    WidgetNode title;
    title.type = "QLabel";
    title.objectName = "tituloLabel";
    title.properties.insert("text", "Meu aplicativo");
    title.properties.insert("alignment", "AlignCenter");
    form.root.children.append(title);

    WidgetNode input;
    input.type = "QLineEdit";
    input.objectName = "inputNome";
    input.properties.insert("placeholderText", "Digite seu nome");
    form.root.children.append(input);

    WidgetNode button;
    button.type = "QPushButton";
    button.objectName = "btnOk";
    button.properties.insert("text", "OK");
    QJsonObject heightBinding;
    heightBinding.insert("$var", "alturaBotao");
    button.properties.insert("minimumHeight", heightBinding);
    EventSpec clicked;
    clicked.signal = "clicked";
    clicked.slot = "on_btnOk_clicked";
    button.events.append(clicked);
    form.root.children.append(button);

    return form;
}

WidgetNode FormModel::nodeFromJson(const QJsonObject &object)
{
    WidgetNode node;
    node.type = object.value("type").toString("QWidget");
    node.objectName = object.value("objectName").toString("widget");
    node.layout = object.value("layout").toString();
    node.properties = object.value("properties").toObject();
    node.events = eventsFromJson(object.value("events"));

    const QJsonArray children = object.value("children").toArray();
    for (const QJsonValue &child : children) {
        if (child.isObject()) {
            node.children.append(nodeFromJson(child.toObject()));
        }
    }
    return node;
}

QJsonObject FormModel::nodeToJson(const WidgetNode &node)
{
    QJsonObject object;
    object.insert("type", node.type);
    object.insert("objectName", node.objectName);
    if (!node.layout.isEmpty()) {
        object.insert("layout", node.layout);
    }
    if (!node.properties.isEmpty()) {
        object.insert("properties", node.properties);
    }
    if (!node.events.isEmpty()) {
        object.insert("events", eventsToJson(node.events));
    }
    if (!node.children.isEmpty()) {
        QJsonArray children;
        for (const WidgetNode &child : node.children) {
            children.append(nodeToJson(child));
        }
        object.insert("children", children);
    }
    return object;
}

FormModel FormModel::fromJson(const QJsonObject &object, QString *error)
{
    FormModel form;
    form.name = object.value("name").toString("MainWindow");
    form.baseClass = object.value("baseClass").toString("QWidget");
    const QJsonValue root = object.value("root");
    if (!root.isObject()) {
        if (error) {
            *error = "Formulário sem objeto 'root'.";
        }
        return form;
    }
    form.root = nodeFromJson(root.toObject());
    return form;
}

QJsonObject FormModel::toJson() const
{
    QJsonObject object;
    object.insert("name", name);
    object.insert("baseClass", baseClass);
    object.insert("root", nodeToJson(root));
    return object;
}
