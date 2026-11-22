#include "FormCanvas.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDrag>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace {
constexpr char kWidgetMime[] = "application/x-cpp-visual-ide-widget";

Qt::Alignment alignmentFromString(const QString &value)
{
    if (value == QStringLiteral("AlignLeft")) {
        return Qt::AlignLeft;
    }
    if (value == QStringLiteral("AlignRight")) {
        return Qt::AlignRight;
    }
    if (value == QStringLiteral("AlignHCenter")) {
        return Qt::AlignHCenter;
    }
    if (value == QStringLiteral("AlignTop")) {
        return Qt::AlignTop;
    }
    if (value == QStringLiteral("AlignBottom")) {
        return Qt::AlignBottom;
    }
    return Qt::AlignCenter;
}
} // namespace

FormCanvas::FormCanvas(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setObjectName(QStringLiteral("formCanvas"));
    setStyleSheet(QStringLiteral("#formCanvas { background: #ffffff; }"));

    layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);
    layout->addStretch(1);
}

void FormCanvas::setForm(const FormModel &form)
{
    formModel = form;
    rebuild();
}

FormModel FormCanvas::form() const
{
    FormModel result = formModel;
    result.root.children = nodes;
    return result;
}

QWidget *FormCanvas::createWidget(const QString &type)
{
    if (type == QStringLiteral("QLabel")) {
        return new QLabel(this);
    }
    if (type == QStringLiteral("QPushButton")) {
        return new QPushButton(this);
    }
    if (type == QStringLiteral("QLineEdit")) {
        return new QLineEdit(this);
    }
    if (type == QStringLiteral("QCheckBox")) {
        return new QCheckBox(this);
    }
    if (type == QStringLiteral("QComboBox")) {
        QComboBox *combo = new QComboBox(this);
        combo->addItems({QStringLiteral("Item 1"), QStringLiteral("Item 2")});
        return combo;
    }
    if (type == QStringLiteral("QSlider")) {
        QSlider *slider = new QSlider(this);
        slider->setOrientation(Qt::Horizontal);
        return slider;
    }
    return new QWidget(this);
}

void FormCanvas::applyProperties(QWidget *widget, const WidgetNode &node) const
{
    for (auto it = node.properties.constBegin(); it != node.properties.constEnd(); ++it) {
        const QString key = it.key();
        const QJsonValue value = it.value();

        if (value.isObject() && value.toObject().contains(QStringLiteral("$var"))) {
            continue;
        }

        QVariant variant;
        if (value.isString()) {
            if (key == QStringLiteral("alignment")) {
                variant = QVariant::fromValue(alignmentFromString(value.toString()));
            } else {
                variant = value.toString();
            }
        } else if (value.isBool()) {
            variant = value.toBool();
        } else if (value.isDouble()) {
            variant = value.toInt();
        } else {
            continue;
        }
        widget->setProperty(key.toUtf8().constData(), variant);
    }
}

void FormCanvas::rebuild()
{
    while (layout->count() > 0) {
        QLayoutItem *item = layout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    widgets.clear();
    nodes.clear();
    selectedWidget = nullptr;
    selectedStyleSheet.clear();

    for (const WidgetNode &node : formModel.root.children) {
        QWidget *widget = createWidget(node.type);
        widget->setObjectName(node.objectName);
        widget->setProperty("nodeIndex", nodes.size());
        applyProperties(widget, node);
        widget->installEventFilter(this);
        layout->addWidget(widget);
        widgets.append(widget);
        nodes.append(node);
    }
    layout->addStretch(1);
    nextId = nodes.size() + 1;
    emit selectionChanged(-1);
}

void FormCanvas::addWidgetType(const QString &type)
{
    WidgetNode node;
    node.type = type;
    node.objectName = QStringLiteral("%1%2").arg(type.toLower()).arg(nextId++);

    if (type == QStringLiteral("QLabel")) {
        node.properties.insert(QStringLiteral("text"), QStringLiteral("Novo título"));
        node.properties.insert(QStringLiteral("alignment"), QStringLiteral("AlignLeft"));
    } else if (type == QStringLiteral("QPushButton")) {
        node.properties.insert(QStringLiteral("text"), QStringLiteral("Botão"));
    } else if (type == QStringLiteral("QLineEdit")) {
        node.properties.insert(QStringLiteral("placeholderText"), QStringLiteral("Digite aqui"));
    } else if (type == QStringLiteral("QCheckBox")) {
        node.properties.insert(QStringLiteral("text"), QStringLiteral("Opção"));
    }

    QWidget *widget = createWidget(type);
    widget->setObjectName(node.objectName);
    widget->setProperty("nodeIndex", nodes.size());
    applyProperties(widget, node);
    widget->installEventFilter(this);

    nodes.append(node);
    widgets.append(widget);
    layout->insertWidget(layout->count() - 1, widget);

    selectWidget(widget);
    emit formModified();
}

void FormCanvas::selectIndex(int index)
{
    if (index >= 0 && index < widgets.size()) {
        selectWidget(widgets.at(index));
    }
}

void FormCanvas::removeSelected()
{
    if (!selectedWidget) {
        return;
    }
    const int index = selectedWidget->property("nodeIndex").toInt();
    if (index < 0 || index >= nodes.size()) {
        return;
    }

    layout->removeWidget(selectedWidget);
    selectedWidget->deleteLater();
    widgets.removeAt(index);
    nodes.removeAt(index);
    selectedWidget = nullptr;
    selectedStyleSheet.clear();

    for (int i = 0; i < widgets.size(); ++i) {
        widgets.at(i)->setProperty("nodeIndex", i);
    }

    emit selectionChanged(-1);
    emit formModified();
}

void FormCanvas::selectWidget(QWidget *widget)
{
    if (selectedWidget == widget) {
        return;
    }
    if (selectedWidget) {
        selectedWidget->setStyleSheet(selectedStyleSheet);
    }

    selectedWidget = widget;
    if (selectedWidget) {
        selectedStyleSheet = selectedWidget->styleSheet();
        selectedWidget->setStyleSheet(
            selectedStyleSheet.isEmpty()
                ? QStringLiteral("border: 2px solid #2d7d9a;")
                : selectedStyleSheet + QStringLiteral("\nborder: 2px solid #2d7d9a;"));
    }
    emit selectionChanged(selectedWidget ? selectedWidget->property("nodeIndex").toInt() : -1);
}

QJsonValue FormCanvas::propertyToJson(const QString &value) const
{
    const QString trimmed = value.trimmed();
    if (trimmed.startsWith(QLatin1Char('$'))) {
        QJsonObject binding;
        binding.insert(QStringLiteral("$var"), trimmed.mid(1));
        return binding;
    }
    if (trimmed == QStringLiteral("true") || trimmed == QStringLiteral("false")) {
        return trimmed == QStringLiteral("true");
    }
    bool isNumber = false;
    const int number = trimmed.toInt(&isNumber);
    if (isNumber) {
        return number;
    }
    return trimmed;
}

void FormCanvas::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(kWidgetMime))) {
        event->acceptProposedAction();
    }
}

void FormCanvas::dropEvent(QDropEvent *event)
{
    const QByteArray data = event->mimeData()->data(QString::fromLatin1(kWidgetMime));
    if (!data.isEmpty()) {
        addWidgetType(QString::fromUtf8(data));
        event->acceptProposedAction();
    }
}

void FormCanvas::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && selectedWidget) {
        removeSelected();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool FormCanvas::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (widget) {
            selectWidget(widget);
        }
    }
    return QWidget::eventFilter(watched, event);
}
