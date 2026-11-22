#include "FormDesigner.h"

#include "FormCanvas.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMimeData>
#include <QPushButton>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
const QString kWidgetMime = QStringLiteral("application/x-cpp-visual-ide-widget");

QString displayValue(const QJsonValue &value)
{
    if (value.isObject() && value.toObject().contains(QStringLiteral("$var"))) {
        return QStringLiteral("$") + value.toObject().value(QStringLiteral("$var")).toString();
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isDouble()) {
        return QString::number(value.toInt());
    }
    return value.toString();
}

QJsonValue parseValue(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.startsWith(QLatin1Char('$'))) {
        QJsonObject binding;
        binding.insert(QStringLiteral("$var"), trimmed.mid(1));
        return binding;
    }
    if (trimmed == QStringLiteral("true") || trimmed == QStringLiteral("false")) {
        return trimmed == QStringLiteral("true");
    }
    bool ok = false;
    const int number = trimmed.toInt(&ok);
    if (ok) {
        return number;
    }
    return trimmed;
}

class WidgetPalette : public QListWidget {
public:
    using QListWidget::QListWidget;

protected:
    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override
    {
        QMimeData *data = QListWidget::mimeData(items);
        if (data && !items.isEmpty()) {
            data->setData(kWidgetMime, items.first()->data(Qt::UserRole).toString().toUtf8());
        }
        return data;
    }
};

QStringList suggestedProperties(const QString &type)
{
    QStringList common{QStringLiteral("toolTip"), QStringLiteral("enabled"),
                       QStringLiteral("minimumWidth"), QStringLiteral("minimumHeight"),
                       QStringLiteral("styleSheet")};
    if (type == QStringLiteral("QLabel")) {
        return QStringList{QStringLiteral("text"), QStringLiteral("alignment")} + common;
    }
    if (type == QStringLiteral("QPushButton") || type == QStringLiteral("QCheckBox")) {
        return QStringList{QStringLiteral("text"), QStringLiteral("checkable"),
                           QStringLiteral("checked")}
               + common;
    }
    if (type == QStringLiteral("QLineEdit")) {
        return QStringList{QStringLiteral("placeholderText"), QStringLiteral("readOnly"),
                           QStringLiteral("text")}
               + common;
    }
    return common;
}
} // namespace

FormDesigner::FormDesigner(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void FormDesigner::setupUi()
{
    auto *splitter = new QSplitter(this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(splitter);

    palette = new WidgetPalette(splitter);
    palette->setDragEnabled(true);
    palette->setDefaultDropAction(Qt::CopyAction);
    palette->setSelectionMode(QAbstractItemView::SingleSelection);
    const QVector<QPair<QString, QString>> items{
        {QStringLiteral("Título (QLabel)"), QStringLiteral("QLabel")},
        {QStringLiteral("Botão (QPushButton)"), QStringLiteral("QPushButton")},
        {QStringLiteral("Campo de texto (QLineEdit)"), QStringLiteral("QLineEdit")},
        {QStringLiteral("Caixa de seleção (QCheckBox)"), QStringLiteral("QCheckBox")},
        {QStringLiteral("Combo (QComboBox)"), QStringLiteral("QComboBox")},
        {QStringLiteral("Slider (QSlider)"), QStringLiteral("QSlider")},
    };
    for (const auto &item : items) {
        auto *listItem = new QListWidgetItem(item.first, palette);
        listItem->setData(Qt::UserRole, item.second);
    }
    connect(palette, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        canvas->addWidgetType(item->data(Qt::UserRole).toString());
    });

    canvas = new FormCanvas(splitter);
    canvas->setMinimumWidth(320);

    auto *inspectorPanel = new QWidget(splitter);
    auto *inspectorLayout = new QVBoxLayout(inspectorPanel);
    inspector = new QTableWidget(inspectorPanel);
    inspector->setColumnCount(2);
    inspector->setHorizontalHeaderLabels({QStringLiteral("Propriedade"), QStringLiteral("Valor")});
    inspector->verticalHeader()->hide();
    inspector->horizontalHeader()->setStretchLastSection(true);
    inspector->setSelectionBehavior(QAbstractItemView::SelectRows);
    inspectorLayout->addWidget(inspector);

    auto *buttons = new QWidget(inspectorPanel);
    auto *buttonsLayout = new QHBoxLayout(buttons);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    addEventButton = new QPushButton(QStringLiteral("+ Evento clicked"), buttons);
    removeButton = new QPushButton(QStringLiteral("Remover"), buttons);
    buttonsLayout->addWidget(addEventButton);
    buttonsLayout->addWidget(removeButton);
    inspectorLayout->addWidget(buttons);

    splitter->setSizes({190, 520, 300});

    connect(canvas, &FormCanvas::selectionChanged, this, &FormDesigner::updateInspector);
    connect(canvas, &FormCanvas::formModified, this, [this]() { emit formChanged(form()); });
    connect(inspector, &QTableWidget::itemChanged, this, &FormDesigner::onPropertyChanged);
    connect(addEventButton, &QPushButton::clicked, this, &FormDesigner::addEvent);
    connect(removeButton, &QPushButton::clicked, this, &FormDesigner::removeSelected);

    updateInspector(-1);
}

void FormDesigner::setForm(const FormModel &form)
{
    canvas->setForm(form);
}

FormModel FormDesigner::form() const
{
    return canvas->form();
}

void FormDesigner::updateInspector(int index)
{
    updatingInspector = true;
    currentIndex = index;
    inspector->setRowCount(0);

    const FormModel current = form();
    if (index < 0 || index >= current.root.children.size()) {
        addEventButton->setEnabled(false);
        removeButton->setEnabled(false);
        updatingInspector = false;
        return;
    }

    addEventButton->setEnabled(true);
    removeButton->setEnabled(true);
    const WidgetNode &node = current.root.children.at(index);

    QStringList keys = suggestedProperties(node.type);
    for (auto it = node.properties.constBegin(); it != node.properties.constEnd(); ++it) {
        if (!keys.contains(it.key())) {
            keys.append(it.key());
        }
    }

    for (const QString &key : keys) {
        const int row = inspector->rowCount();
        inspector->insertRow(row);
        auto *nameItem = new QTableWidgetItem(key);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        inspector->setItem(row, 0, nameItem);
        inspector->setItem(row, 1, new QTableWidgetItem(displayValue(node.properties.value(key))));
    }

    updatingInspector = false;
}

void FormDesigner::onPropertyChanged(QTableWidgetItem *item)
{
    if (updatingInspector || !item || item->column() != 1 || currentIndex < 0) {
        return;
    }
    auto *nameItem = inspector->item(item->row(), 0);
    if (!nameItem) {
        return;
    }

    FormModel current = form();
    if (currentIndex >= current.root.children.size()) {
        return;
    }
    WidgetNode node = current.root.children.at(currentIndex);
    node.properties.insert(nameItem->text(), parseValue(item->text()));
    current.root.children[currentIndex] = node;

    setForm(current);
    canvas->selectIndex(currentIndex);
    emit formChanged(form());
}

void FormDesigner::addEvent()
{
    FormModel current = form();
    if (currentIndex < 0 || currentIndex >= current.root.children.size()) {
        return;
    }
    WidgetNode node = current.root.children.at(currentIndex);
    const QString slotName = QStringLiteral("on_%1_clicked").arg(node.objectName);
    for (const EventSpec &event : node.events) {
        if (event.signal == QStringLiteral("clicked") && event.slot == slotName) {
            return;
        }
    }
    EventSpec event;
    event.signal = QStringLiteral("clicked");
    event.slot = slotName;
    node.events.append(event);
    current.root.children[currentIndex] = node;

    setForm(current);
    canvas->selectIndex(currentIndex);
    emit formChanged(form());
}

void FormDesigner::removeSelected()
{
    canvas->removeSelected();
}
