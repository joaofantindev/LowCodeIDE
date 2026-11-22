#pragma once

#include "Project.h"

#include <QMainWindow>

class QDockWidget;
class FormDesigner;
class QPlainTextEdit;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void openProjectPath(const QString &path);

private slots:
    void newProject();
    void openProject();
    void saveProject();
    void regenerateCode();
    void buildProject();
    void runProject();
    void openFileFromTree(QTreeWidgetItem *item, int column);
    void closeTab(int index);

private:
    void setupUi();
    void loadProject(const Project &project);
    void refreshProjectTree();
    void addTreeItems(QTreeWidgetItem *parent, const QString &path);
    void openDocument(const QString &filePath);
    void appendOutput(const QString &text);
    QString findExecutable(const QString &name) const;

    Project project;
    FormDesigner *designer = nullptr;
    QTabWidget *editorTabs = nullptr;
    QTreeWidget *projectTree = nullptr;
    QPlainTextEdit *outputView = nullptr;
    QDockWidget *outputDock = nullptr;
};
