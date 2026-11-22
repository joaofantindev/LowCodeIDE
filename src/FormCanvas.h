#pragma once

#include "FormModel.h"

#include <QWidget>

class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QKeyEvent;
class QVBoxLayout;

class FormCanvas : public QWidget {
    Q_OBJECT

public:
    explicit FormCanvas(QWidget *parent = nullptr);

    void setForm(const FormModel &form);
    FormModel form() const;

    void addWidgetType(const QString &type);
    void removeSelected();
    void selectIndex(int index);

signals:
    void selectionChanged(int index);
    void formModified();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuild();
    QWidget *createWidget(const QString &type);
    void applyProperties(QWidget *widget, const WidgetNode &node) const;
    void selectWidget(QWidget *widget);
    QJsonValue propertyToJson(const QString &value) const;

    FormModel formModel;
    QVector<WidgetNode> nodes;
    QVector<QWidget *> widgets;
    QVBoxLayout *layout = nullptr;
    QWidget *selectedWidget = nullptr;
    QString selectedStyleSheet;
    int nextId = 1;
};
