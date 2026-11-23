#include "UiForm.h"

#include <QJsonObject>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

struct Connection {
    QString sender;
    QString signal;
    QString receiver;
    QString slot;
};

void writeProperty(QXmlStreamWriter &xml, const QString &name, const QJsonValue &value)
{
    if (name == QStringLiteral("objectName")) {
        return;
    }
    xml.writeStartElement(QStringLiteral("property"));
    xml.writeAttribute(QStringLiteral("name"), name);
    if (value.isBool()) {
        xml.writeTextElement(QStringLiteral("bool"),
                             value.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
    } else if (value.isDouble()) {
        xml.writeTextElement(QStringLiteral("number"), QString::number(value.toInt()));
    } else if (value.isObject()) {
        // Bindings vivem no project.json; aqui fica um valor neutro.
        xml.writeTextElement(QStringLiteral("number"), QStringLiteral("0"));
    } else {
        const QString text = value.toString();
        if (name == QStringLiteral("alignment")) {
            xml.writeTextElement(QStringLiteral("set"),
                                 text.startsWith(QStringLiteral("Qt::"))
                                     ? text
                                     : QStringLiteral("Qt::") + text);
        } else {
            xml.writeTextElement(QStringLiteral("string"), text);
        }
    }
    xml.writeEndElement();
}

void writeGeometry(QXmlStreamWriter &xml, const WidgetNode &node)
{
    const auto value = [&node](const QString &key, int fallback) {
        const QJsonValue property = node.properties.value(key);
        return property.isDouble() ? property.toInt() : fallback;
    };
    xml.writeStartElement(QStringLiteral("property"));
    xml.writeAttribute(QStringLiteral("name"), QStringLiteral("geometry"));
    xml.writeStartElement(QStringLiteral("rect"));
    xml.writeTextElement(QStringLiteral("x"), QString::number(value(QStringLiteral("left"), 0)));
    xml.writeTextElement(QStringLiteral("y"), QString::number(value(QStringLiteral("top"), 0)));
    xml.writeTextElement(QStringLiteral("width"),
                         QString::number(value(QStringLiteral("width"), 120)));
    xml.writeTextElement(QStringLiteral("height"),
                         QString::number(value(QStringLiteral("height"), 32)));
    xml.writeEndElement();
    xml.writeEndElement();
}

void writeWidget(QXmlStreamWriter &xml, const WidgetNode &node)
{
    xml.writeStartElement(QStringLiteral("widget"));
    xml.writeAttribute(QStringLiteral("class"), node.type.isEmpty() ? QStringLiteral("QWidget")
                                                                    : node.type);
    xml.writeAttribute(QStringLiteral("name"), node.objectName);

    if (node.layout.isEmpty()) {
        writeGeometry(xml, node);
    }
    for (auto it = node.properties.constBegin(); it != node.properties.constEnd(); ++it) {
        if (it.key() == QStringLiteral("left") || it.key() == QStringLiteral("top")
            || it.key() == QStringLiteral("width") || it.key() == QStringLiteral("height")) {
            continue;
        }
        writeProperty(xml, it.key(), it.value());
    }

    if (!node.layout.isEmpty()) {
        xml.writeStartElement(QStringLiteral("layout"));
        xml.writeAttribute(QStringLiteral("class"), node.layout);
        xml.writeAttribute(QStringLiteral("name"), node.objectName + QStringLiteral("Layout"));
        for (const WidgetNode &child : node.children) {
            xml.writeStartElement(QStringLiteral("item"));
            writeWidget(xml, child);
            xml.writeEndElement();
        }
        xml.writeEndElement();
    } else {
        for (const WidgetNode &child : node.children) {
            writeWidget(xml, child);
        }
    }

    xml.writeEndElement();
}

void collectConnections(const WidgetNode &node, QVector<Connection> &connections)
{
    for (const EventSpec &event : node.events) {
        connections.append({node.objectName, event.signal, QString(), event.slot});
    }
    for (const WidgetNode &child : node.children) {
        collectConnections(child, connections);
    }
}

WidgetNode *findNode(WidgetNode &node, const QString &objectName)
{
    if (node.objectName == objectName) {
        return &node;
    }
    for (WidgetNode &child : node.children) {
        if (WidgetNode *found = findNode(child, objectName)) {
            return found;
        }
    }
    return nullptr;
}

QString cleanSignature(const QString &text)
{
    const int index = text.indexOf(QLatin1Char('('));
    return (index < 0 ? text : text.left(index)).trimmed();
}

} // namespace

QString UiForm::toUi(const FormModel &form)
{
    QString xml;
    QXmlStreamWriter stream(&xml);
    stream.setAutoFormatting(true);
    stream.setAutoFormattingIndent(1);
    stream.writeStartDocument();
    stream.writeStartElement(QStringLiteral("ui"));
    stream.writeAttribute(QStringLiteral("version"), QStringLiteral("4.0"));
    stream.writeTextElement(QStringLiteral("class"), form.name);

    writeWidget(stream, form.root);

    stream.writeStartElement(QStringLiteral("resources"));
    stream.writeEndElement();

    QVector<Connection> connections;
    collectConnections(form.root, connections);
    stream.writeStartElement(QStringLiteral("connections"));
    for (const Connection &connection : connections) {
        stream.writeStartElement(QStringLiteral("connection"));
        stream.writeTextElement(QStringLiteral("sender"), connection.sender);
        stream.writeTextElement(QStringLiteral("signal"), connection.signal + QStringLiteral("()"));
        stream.writeTextElement(QStringLiteral("receiver"), form.name);
        stream.writeTextElement(QStringLiteral("slot"), connection.slot + QStringLiteral("()"));
        stream.writeEndElement();
    }
    stream.writeEndElement();

    stream.writeStartElement(QStringLiteral("slots"));
    for (const Connection &connection : connections) {
        stream.writeTextElement(QStringLiteral("slot"), connection.slot + QStringLiteral("()"));
    }
    stream.writeEndElement();

    stream.writeEndElement();
    stream.writeEndDocument();
    return xml;
}

FormModel UiForm::fromUi(const QString &xml, QString *error)
{
    FormModel form;
    form.name.clear(); // FormModel começa com "MainWindow"; o .ui é a fonte do nome.
    QXmlStreamReader reader(xml);
    QVector<WidgetNode *> stack;
    QString currentProperty;
    Connection connection;
    bool inConnections = false;

    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();

        if (reader.isEndElement()) {
            const QString element = reader.name().toString();
            if (element == QStringLiteral("widget") && !stack.isEmpty()) {
                stack.removeLast();
            } else if (element == QStringLiteral("property")) {
                currentProperty.clear();
            } else if (element == QStringLiteral("connections")) {
                inConnections = false;
            } else if (element == QStringLiteral("connection") && inConnections) {
                WidgetNode *sender = findNode(form.root, connection.sender);
                if (sender && !connection.signal.isEmpty() && !connection.slot.isEmpty()) {
                    const EventSpec event{cleanSignature(connection.signal),
                                          cleanSignature(connection.slot)};
                    bool exists = false;
                    for (const EventSpec &current : sender->events) {
                        if (current.signal == event.signal && current.slot == event.slot) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        sender->events.append(event);
                    }
                }
                connection = Connection();
            }
            continue;
        }

        if (!reader.isStartElement()) {
            continue;
        }

        const QString element = reader.name().toString();
        WidgetNode *current = stack.isEmpty() ? nullptr : stack.last();

        if (element == QStringLiteral("class") && stack.isEmpty() && form.name.isEmpty()) {
            form.name = reader.readElementText();
            continue;
        }
        if (element == QStringLiteral("widget")) {
            WidgetNode node;
            node.type = reader.attributes().value(QStringLiteral("class")).toString();
            node.objectName = reader.attributes().value(QStringLiteral("name")).toString();
            if (node.type.isEmpty()) {
                node.type = QStringLiteral("QWidget");
            }
            if (stack.isEmpty()) {
                form.root = node;
                stack.append(&form.root);
            } else {
                current->children.append(node);
                stack.append(&current->children.last());
            }
            continue;
        }
        if (element == QStringLiteral("layout") && current) {
            const QString layoutClass = reader.attributes().value(QStringLiteral("class")).toString();
            if (!layoutClass.isEmpty()) {
                current->layout = layoutClass;
            }
            continue;
        }
        if (element == QStringLiteral("property") && current) {
            currentProperty = reader.attributes().value(QStringLiteral("name")).toString();
            if (currentProperty == QStringLiteral("geometry")) {
                QJsonObject rect;
                while (!reader.atEnd()
                       && !(reader.isEndElement() && reader.name() == QStringLiteral("property"))) {
                    reader.readNext();
                    if (reader.isStartElement()) {
                        const QString tag = reader.name().toString();
                        if (tag == QStringLiteral("x") || tag == QStringLiteral("y")
                            || tag == QStringLiteral("width") || tag == QStringLiteral("height")) {
                            rect.insert(tag, reader.readElementText().toInt());
                        }
                    }
                }
                current->properties.insert(QStringLiteral("left"),
                                           rect.value(QStringLiteral("x")).toInt(0));
                current->properties.insert(QStringLiteral("top"),
                                           rect.value(QStringLiteral("y")).toInt(0));
                current->properties.insert(QStringLiteral("width"),
                                           rect.value(QStringLiteral("width")).toInt(120));
                current->properties.insert(QStringLiteral("height"),
                                           rect.value(QStringLiteral("height")).toInt(32));
                currentProperty.clear();
            }
            continue;
        }
        if (!currentProperty.isEmpty() && current
            && (element == QStringLiteral("string") || element == QStringLiteral("number")
                || element == QStringLiteral("bool") || element == QStringLiteral("set"))) {
            const QString text = reader.readElementText();
            if (element == QStringLiteral("number")) {
                current->properties.insert(currentProperty, text.toInt());
            } else if (element == QStringLiteral("bool")) {
                current->properties.insert(currentProperty, text == QStringLiteral("true"));
            } else {
                current->properties.insert(currentProperty, text);
            }
            currentProperty.clear();
            continue;
        }
        if (element == QStringLiteral("connections")) {
            inConnections = true;
            continue;
        }
        if (inConnections && (element == QStringLiteral("sender")
                              || element == QStringLiteral("signal")
                              || element == QStringLiteral("receiver")
                              || element == QStringLiteral("slot"))) {
            const QString text = reader.readElementText();
            if (element == QStringLiteral("sender")) {
                connection.sender = text;
            } else if (element == QStringLiteral("signal")) {
                connection.signal = text;
            } else if (element == QStringLiteral("receiver")) {
                connection.receiver = text;
            } else {
                connection.slot = text;
            }
        }
    }

    if (reader.hasError() && error) {
        *error = reader.errorString();
    }
    return form;
}
