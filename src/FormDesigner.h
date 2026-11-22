#pragma once

#include "FormModel.h"

#include <QWidget>

class QListWidget;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class FormCanvas;

class FormDesigner : public QWidget {
    Q_OBJECT

public:
    explicit FormDesigner(QWidget *parent = nullptr);

    void setForm(const FormModel &form);
    FormModel form() const;

signals:
    void formChanged(const FormModel &form);

private slots:
    void updateInspector(int index);
    void onPropertyChanged(QTableWidgetItem *item);
    void addEvent();
    void removeSelected();

private:
    void setupUi();

    FormCanvas *canvas = nullptr;
    QListWidget *palette = nullptr;
    QTableWidget *inspector = nullptr;
    QPushButton *addEventButton = nullptr;
    QPushButton *removeButton = nullptr;
    bool updatingInspector = false;
    int currentIndex = -1;
};
