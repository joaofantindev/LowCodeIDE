#pragma once

#include "LspClient.h"
#include "Project.h"

#include <QHash>
#include <QMainWindow>

class QDockWidget;
class QFileSystemWatcher;
class QFrame;
class FormDesigner;
class QLabel;
class QPlainTextEdit;
class QProcess;
class QStackedWidget;
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
    void newScratchFile();
    void openCppFile();
    void saveProject();
    void regenerateCode();
    void buildProject();
    void runProject();
    void runScratchFile();
    void stopRun();
    void openFileFromTree(QTreeWidgetItem *item, int column);
    void closeTab(int index);
    void requestCompletion();
    void requestDefinition();
    void requestHover();
    void onDiagnostics(const QString &filePath, const QVector<LspDiagnostic> &diagnostics);
    void reopenDocumentsInLsp();
    void editInterface();
    void onUiFileChanged(const QString &path);
    void cancelBuild();

private:
    void setupUi();
    void buildWelcomePage();
    void applyTheme();
    void loadProject(const Project &project);
    void refreshProjectTree();
    void addTreeItems(QTreeWidgetItem *parent, const QString &path);
    void addScratchItems(QTreeWidgetItem *parent);
    void openDocument(const QString &filePath);
    QString currentSourcePath() const;
    void appendOutput(const QString &text);
    void appendTerminal(const QString &text);
    void appendBuildOutput(const QString &text);
    void finishBuild(bool success);
    qint64 launchInTerminal(const QString &executable, const QString &workingDirectory);
    QString findExecutable(const QString &name) const;
    void syncUiWatcher();
    void reloadUiFromDisk(const QString &path);

    Project project;
    FormDesigner *designer = nullptr;
    LspClient *lsp = nullptr;
    QProcess *buildProcess = nullptr;
    QProcess *scratchProcess = nullptr;
    QStackedWidget *centralStack = nullptr;
    QFrame *welcomePage = nullptr;
    QTabWidget *editorTabs = nullptr;
    QTabWidget *outputTabs = nullptr;
    QTreeWidget *projectTree = nullptr;
    QPlainTextEdit *outputView = nullptr;
    QPlainTextEdit *terminalView = nullptr;
    QPlainTextEdit *buildView = nullptr;
    QDockWidget *outputDock = nullptr;
    bool runAfterBuild = false;
    bool buildCancelled = false;
    bool buildHadCompileCommands = false;
    int buildPhase = 0; // 0 = parado, 1 = cmake -S, 2 = cmake --build
    qint64 runPid = 0;
    QString scratchDir;
    QFileSystemWatcher *uiWatcher = nullptr;
    QHash<QString, QByteArray> uiHashes;
};
