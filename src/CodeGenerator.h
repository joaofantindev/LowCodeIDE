#pragma once

#include "Project.h"

#include <QString>
#include <QStringList>

struct GenerateResult {
    bool ok = false;
    QString error;
    QStringList writtenFiles;
};

class CodeGenerator {
public:
    static GenerateResult generateAll(const Project &project, bool overwriteUserFiles = false);
};
