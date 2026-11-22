#pragma once

#include "FormModel.h"

#include <QString>

class UiForm {
public:
    static QString toUi(const FormModel &form);
    static FormModel fromUi(const QString &xml, QString *error = nullptr);
};
