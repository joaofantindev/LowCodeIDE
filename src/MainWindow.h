#pragma once

#include "LspClient.h"
#include "Project.h"

#include <QHash>
#include <QMainWindow>

class QDockWidget;
class QFileSystemWatcher;
class FormDesigner;
class QPlainTextEdit;
class QProcess;
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
    void stopRun();
    void openFileFromTree(QTreeWidgetItem *item, int column);
    void closeTab(int index);
    void requestCompletion();
    void requestDefinition();
    void requestHover();
    void onDiagnostics(const QString &filePath, const QVector<LspDiagnostic> &diagnostics);
    void reopenDocumentsInLsp();
    void toggleDarkTheme(bool enabled);
    void editInterface();
    void onUiFileChanged(const QString &path);

private:
    void setupUi();
    void loadProject(const Project &project);
    void refreshProjectTree();
    void addTreeItems(QTreeWidgetItem *parent, const QString &path);
    void openDocument(const QString &filePath);
    void appendOutput(const QString &text);
    void appendTerminal(const QString &text);
    QString findExecutable(const QString &name) const;
    void syncUiWatcher();
    void reloadUiFromDisk(const QString &path);

    Project project;
    FormDesigner *designer = nullptr;
    LspClient *lsp = nullptr;
    QProcess *runProcess = nullptr;
    QTabWidget *editorTabs = nullptr;
    QTabWidget *outputTabs = nullptr;
    QTreeWidget *projectTree = nullptr;
    QPlainTextEdit *outputView = nullptr;
    QPlainTextEdit *terminalView = nullptr;
    QDockWidget *outputDock = nullptr;
    QFileSystemWatcher *uiWatcher = nullptr;
    QHash<QString, QByteArray> uiHashes;
};
