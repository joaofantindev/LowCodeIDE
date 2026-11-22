#include "MainWindow.h"

#include "CodeEditor.h"
#include "CodeGenerator.h"
#include "FormDesigner.h"
#include "SyntaxHighlighter.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QInputDialog>
#include <QCursor>
#include <QCryptographicHash>
#include <QKeySequence>
#include <QMenuBar>
#include <QPalette>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTextCursor>
#include <QToolTip>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("C++ Visual IDE"));
    resize(1250, 820);

    editorTabs = new QTabWidget(this);
    editorTabs->setTabsClosable(true);
    setCentralWidget(editorTabs);
    connect(editorTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    auto *treeDock = new QDockWidget(QStringLiteral("Projeto"), this);
    treeDock->setObjectName(QStringLiteral("projectDock"));
    projectTree = new QTreeWidget(treeDock);
    projectTree->setHeaderLabels({QStringLiteral("Arquivos")});
    projectTree->setAlternatingRowColors(true);
    treeDock->setWidget(projectTree);
    addDockWidget(Qt::LeftDockWidgetArea, treeDock);
    connect(projectTree, &QTreeWidget::itemDoubleClicked, this, &MainWindow::openFileFromTree);

    outputDock = new QDockWidget(QStringLiteral("Saída"), this);
    outputDock->setObjectName(QStringLiteral("outputDock"));
    outputTabs = new QTabWidget(outputDock);
    outputView = new QPlainTextEdit(outputTabs);
    outputView->setReadOnly(true);
    outputView->setMaximumBlockCount(2000);
    terminalView = new QPlainTextEdit(outputTabs);
    terminalView->setReadOnly(true);
    terminalView->setMaximumBlockCount(5000);
    terminalView->setFont(QFont(QStringLiteral("Consolas"), 10));
    buildView = new QPlainTextEdit(outputTabs);
    buildView->setReadOnly(true);
    buildView->setMaximumBlockCount(5000);
    buildView->setFont(QFont(QStringLiteral("Consolas"), 10));
    outputTabs->addTab(outputView, QStringLiteral("Saída"));
    outputTabs->addTab(terminalView, QStringLiteral("Terminal"));
    outputTabs->addTab(buildView, QStringLiteral("Build"));
    outputDock->setWidget(outputTabs);
    addDockWidget(Qt::BottomDockWidgetArea, outputDock);

    QToolBar *projectToolbar = addToolBar(QStringLiteral("Projeto"));
    projectToolbar->setObjectName(QStringLiteral("projectToolbar"));

    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&Arquivo"));

    QAction *newAction = fileMenu->addAction(QStringLiteral("&Novo Projeto..."));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newProject);

    QAction *openAction = fileMenu->addAction(QStringLiteral("&Abrir Projeto..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openProject);

    QAction *saveAction = fileMenu->addAction(QStringLiteral("&Salvar"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveProject);

    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(QStringLiteral("S&air"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *projectMenu = menuBar()->addMenu(QStringLiteral("&Projeto"));

    QAction *designerAction = projectMenu->addAction(QStringLiteral("&Editar interface (Qt Designer)"));
    designerAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    connect(designerAction, &QAction::triggered, this, &MainWindow::editInterface);
    projectToolbar->addAction(designerAction);

    QAction *generateAction = projectMenu->addAction(QStringLiteral("&Gerar código"));
    generateAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    connect(generateAction, &QAction::triggered, this, &MainWindow::regenerateCode);
    projectToolbar->addAction(generateAction);

    QAction *buildAction = projectMenu->addAction(QStringLiteral("&Compilar"));
    buildAction->setShortcut(QKeySequence(QStringLiteral("F7")));
    connect(buildAction, &QAction::triggered, this, &MainWindow::buildProject);
    projectToolbar->addAction(buildAction);

    QAction *cancelBuildAction = projectMenu->addAction(QStringLiteral("Cancelar &build"));
    cancelBuildAction->setShortcut(QKeySequence(QStringLiteral("Shift+F7")));
    connect(cancelBuildAction, &QAction::triggered, this, &MainWindow::cancelBuild);

    QAction *runAction = projectMenu->addAction(QStringLiteral("&Executar"));
    runAction->setShortcut(QKeySequence(QStringLiteral("F5")));
    connect(runAction, &QAction::triggered, this, &MainWindow::runProject);
    projectToolbar->addAction(runAction);

    QAction *stopAction = projectMenu->addAction(QStringLiteral("&Parar execução"));
    stopAction->setShortcut(QKeySequence(QStringLiteral("Shift+F5")));
    connect(stopAction, &QAction::triggered, this, &MainWindow::stopRun);
    projectToolbar->addAction(stopAction);

    QMenu *codeMenu = menuBar()->addMenu(QStringLiteral("&Código"));
    QAction *completeAction = codeMenu->addAction(QStringLiteral("&Autocomplete"));
    completeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Space")));
    connect(completeAction, &QAction::triggered, this, &MainWindow::requestCompletion);

    QAction *definitionAction = codeMenu->addAction(QStringLiteral("&Ir para definição"));
    definitionAction->setShortcut(QKeySequence(QStringLiteral("F2")));
    connect(definitionAction, &QAction::triggered, this, &MainWindow::requestDefinition);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("&Exibir"));
    QAction *darkAction = viewMenu->addAction(QStringLiteral("Tema &escuro"));
    darkAction->setCheckable(true);
    connect(darkAction, &QAction::toggled, this, &MainWindow::toggleDarkTheme);

    statusBar()->showMessage(QStringLiteral("Pronto. Crie ou abra um projeto para começar."));
}

void MainWindow::appendOutput(const QString &text)
{
    outputView->appendPlainText(text);
    outputDock->show();
    outputDock->raise();
}

void MainWindow::appendTerminal(const QString &text)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    if (lines.size() > 1 && lines.last().isEmpty()) {
        lines.removeLast();
    }
    for (const QString &line : lines) {
        if (!line.isEmpty()) {
            terminalView->appendPlainText(line);
        }
    }
    outputDock->show();
    outputTabs->setCurrentWidget(terminalView);
}

void MainWindow::appendBuildOutput(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (!line.trimmed().isEmpty()) {
            buildView->appendPlainText(line);
        }
    }
}

QString MainWindow::findExecutable(const QString &name) const
{
    const QString found = QStandardPaths::findExecutable(name);
    if (!found.isEmpty()) {
        return found;
    }
#ifdef Q_OS_WIN
    const QString fallback = QStringLiteral("C:/msys64/mingw64/bin/%1.exe").arg(name);
    if (QFile::exists(fallback)) {
        return fallback;
    }
#endif
    return {};
}

void MainWindow::newProject()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Novo projeto"),
                                               QStringLiteral("Nome do projeto:"),
                                               QLineEdit::Normal, QStringLiteral("MeuProjeto"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    const QString parentDir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Pasta onde criar o projeto"), QDir::homePath());
    if (parentDir.isEmpty()) {
        return;
    }

    const QString projectName = name.trimmed();
    const QString rootPath = QDir::cleanPath(parentDir + QLatin1Char('/') + projectName);
    if (QDir(rootPath).exists() && !QDir(rootPath).entryList(QDir::NoDotAndDotDot).isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Novo projeto"),
                             QStringLiteral("A pasta já existe e não está vazia:\n%1").arg(rootPath));
        return;
    }

    const Project created = Project::create(rootPath, projectName);
    QString error;
    if (!created.save(&error)) {
        QMessageBox::critical(this, QStringLiteral("Novo projeto"), error);
        return;
    }
    const GenerateResult result = CodeGenerator::generateAll(created);
    if (!result.ok) {
        QMessageBox::critical(this, QStringLiteral("Novo projeto"), result.error);
        return;
    }

    loadProject(created);
    appendOutput(QStringLiteral("Projeto criado em %1").arg(rootPath));
    appendOutput(QStringLiteral("Arquivos gerados: %1").arg(result.writtenFiles.count()));
}

void MainWindow::openProject()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Abrir projeto"), QDir::homePath(),
        QStringLiteral("Projeto C++ (project.json);;Todos os arquivos (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    openProjectPath(path);
}

void MainWindow::openProjectPath(const QString &path)
{
    QString error;
    const Project loaded = Project::load(path, &error);
    if (!error.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Abrir projeto"), error);
        return;
    }
    loadProject(loaded);
    appendOutput(QStringLiteral("Projeto aberto: %1").arg(path));
}

void MainWindow::loadProject(const Project &loaded)
{
    project = loaded;
    refreshProjectTree();

    if (!lsp) {
        lsp = new LspClient(this);
        connect(lsp, &LspClient::diagnosticsReady, this, &MainWindow::onDiagnostics);
        connect(lsp, &LspClient::message, this,
                [this](const QString &text) { appendOutput(QStringLiteral("[clangd] %1").arg(text)); });
        connect(lsp, &LspClient::started, this, [this](bool ok) {
            if (ok) {
                reopenDocumentsInLsp();
            }
        });
    }
    if (!project.rootPath.isEmpty()) {
        lsp->start(project.rootPath);
    }

    while (editorTabs->count() > 0) {
        editorTabs->removeTab(0);
    }

    if (!project.forms.isEmpty()) {
        if (!designer) {
            designer = new FormDesigner(editorTabs);
            connect(designer, &FormDesigner::formChanged, this, [this](const FormModel &form) {
                if (!project.forms.isEmpty()) {
                    project.forms[0] = form;
                }
            });
        }
        designer->setForm(project.forms.first());
        editorTabs->insertTab(0, designer, QStringLiteral("Designer"));
        editorTabs->setCurrentIndex(0);
    }

    QStringList initialFiles{
        QStringLiteral("src/main.cpp"),
        QStringLiteral("src/MainWindow.cpp"),
        QStringLiteral("src/MainWindow.h"),
        QStringLiteral("generated/bindings.h"),
        QStringLiteral("generated/app_vars.h"),
        QStringLiteral("project.json"),
    };
    if (!project.forms.isEmpty()) {
        initialFiles.prepend(QStringLiteral("forms/%1.ui").arg(project.forms.first().name));
    }
    for (const QString &relative : initialFiles) {
        const QString path = QDir(project.rootPath).filePath(relative);
        if (QFileInfo::exists(path)) {
            openDocument(path);
        }
    }
    syncUiWatcher();
    statusBar()->showMessage(QStringLiteral("Projeto: %1").arg(project.rootPath));
}

void MainWindow::refreshProjectTree()
{
    projectTree->clear();
    auto *root = new QTreeWidgetItem(projectTree, {project.name.isEmpty() ? QStringLiteral("Projeto")
                                                                          : project.name});
    root->setData(0, Qt::UserRole, project.rootPath);
    root->setFont(0, QFont(QApplication::font().family(), QApplication::font().pointSize(), QFont::Bold));
    addTreeItems(root, project.rootPath);
    projectTree->addTopLevelItem(root);
    projectTree->expandItem(root);
}

void MainWindow::addTreeItems(QTreeWidgetItem *parent, const QString &path)
{
    const QFileInfoList entries = QDir(path).entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &info : entries) {
        if (info.fileName().startsWith(QLatin1Char('.')) || info.fileName() == QStringLiteral("build")) {
            continue;
        }
        auto *item = new QTreeWidgetItem(parent, {info.fileName()});
        item->setData(0, Qt::UserRole, info.absoluteFilePath());
        if (info.isDir()) {
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            addTreeItems(item, info.absoluteFilePath());
        } else {
            item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        }
    }
}

void MainWindow::openFileFromTree(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) {
        return;
    }
    const QString path = item->data(0, Qt::UserRole).toString();
    if (!path.isEmpty() && QFileInfo::exists(path) && QFileInfo(path).isFile()) {
        openDocument(path);
    }
}

void MainWindow::openDocument(const QString &filePath)
{
    for (int i = 0; i < editorTabs->count(); ++i) {
        if (editorTabs->tabBar()->tabData(i).toString() == filePath) {
            editorTabs->setCurrentIndex(i);
            return;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        appendOutput(QStringLiteral("Não foi possível abrir %1: %2").arg(filePath, file.errorString()));
        return;
    }
    const QByteArray content = file.readAll();

    auto *editor = new CodeEditor(editorTabs);
    editor->setFilePath(filePath);
    editor->setFont(QFont(QStringLiteral("Consolas"), 10));
    editor->setPlainText(QString::fromUtf8(content));
    editor->document()->setModified(false);
    new SyntaxHighlighter(editor->document());

    connect(editor, &CodeEditor::completionRequested, this, &MainWindow::requestCompletion);
    connect(editor, &CodeEditor::definitionRequested, this, &MainWindow::requestDefinition);
    connect(editor, &CodeEditor::hoverRequested, this, &MainWindow::requestHover);
    connect(editor, &QPlainTextEdit::textChanged, this, [this, editor]() {
        if (lsp && lsp->isInitialized() && !editor->filePath().isEmpty()) {
            lsp->changeFile(editor->filePath(), editor->toPlainText());
        }
    });
    if (lsp && lsp->isInitialized()) {
        lsp->openFile(filePath, editor->toPlainText());
    }

    const int index = editorTabs->addTab(editor, QFileInfo(filePath).fileName());
    editorTabs->tabBar()->setTabData(index, filePath);
    editorTabs->setTabToolTip(index, filePath);
    editorTabs->setCurrentIndex(index);
}

void MainWindow::closeTab(int index)
{
    auto *editor = qobject_cast<CodeEditor *>(editorTabs->widget(index));
    if (editor && editor->document()->isModified()) {
        const QString path = editorTabs->tabBar()->tabData(index).toString();
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Salvar alterações"),
            QStringLiteral("Salvar as alterações em %1?").arg(QFileInfo(path).fileName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (answer == QMessageBox::Cancel) {
            return;
        }
        if (answer == QMessageBox::Save) {
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(editor->toPlainText().toUtf8());
                file.commit();
            }
        }
    }
    const QString path = editorTabs->tabBar()->tabData(index).toString();
    if (lsp && !path.isEmpty()) {
        lsp->closeFile(path);
    }
    editorTabs->removeTab(index);
}

void MainWindow::saveProject()
{
    for (int i = 0; i < editorTabs->count(); ++i) {
        auto *editor = qobject_cast<CodeEditor *>(editorTabs->widget(i));
        const QString path = editorTabs->tabBar()->tabData(i).toString();
        if (!editor || path.isEmpty() || !editor->document()->isModified()) {
            continue;
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            appendOutput(QStringLiteral("Falha ao salvar %1: %2").arg(path, file.errorString()));
            continue;
        }
        file.write(editor->toPlainText().toUtf8());
        if (file.commit()) {
            editor->document()->setModified(false);
        }
    }

    if (!project.rootPath.isEmpty()) {
        QString error;
        if (!project.save(&error)) {
            appendOutput(error);
        } else {
            syncUiWatcher();
            statusBar()->showMessage(QStringLiteral("Projeto salvo."), 3000);
        }
    }
}

void MainWindow::regenerateCode()
{
    if (project.rootPath.isEmpty() || project.forms.isEmpty()) {
        appendOutput(QStringLiteral("Abra ou crie um projeto antes de gerar código."));
        return;
    }
    bool needsMigration = false;
    for (const FormModel &form : project.forms) {
        const QString ui = QDir(project.rootPath)
                               .filePath(QStringLiteral("forms/%1.ui").arg(form.name));
        if (!QFileInfo::exists(ui)) {
            needsMigration = true;
        }
    }
    QString error;
    if (!project.save(&error)) {
        appendOutput(error);
        return;
    }
    syncUiWatcher();
    if (needsMigration) {
        appendOutput(QStringLiteral("Formulários antigos migrados para .ui."));
    }
    const GenerateResult result = CodeGenerator::generateAll(project);
    if (!result.ok) {
        appendOutput(QStringLiteral("Erro ao gerar código: %1").arg(result.error));
        return;
    }

    for (int i = 0; i < editorTabs->count(); ++i) {
        const QString path = editorTabs->tabBar()->tabData(i).toString();
        if (!result.writtenFiles.contains(path)) {
            continue;
        }
        auto *editor = qobject_cast<CodeEditor *>(editorTabs->widget(i));
        if (!editor) {
            continue;
        }
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            editor->setPlainText(QString::fromUtf8(file.readAll()));
            editor->document()->setModified(false);
        }
    }

    refreshProjectTree();
    appendOutput(QStringLiteral("Código gerado: %1 arquivo(s).").arg(result.writtenFiles.count()));
}

void MainWindow::buildProject()
{
    if (buildProcess && buildProcess->state() != QProcess::NotRunning) {
        cancelBuild();
        return;
    }
    if (project.rootPath.isEmpty()) {
        appendOutput(QStringLiteral("Abra ou crie um projeto antes de compilar."));
        return;
    }

    if (!project.forms.isEmpty()) {
        regenerateCode();
    }

    const QString compileCommands = QDir(project.rootPath)
                                        .filePath(QStringLiteral("build/compile_commands.json"));
    buildHadCompileCommands = QFileInfo::exists(compileCommands);

    const QString cmake = findExecutable(QStringLiteral("cmake"));
    if (cmake.isEmpty()) {
        appendOutput(QStringLiteral("cmake não encontrado no PATH."));
        return;
    }

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_WIN
    const QString msys = QStringLiteral("C:/msys64/mingw64");
    if (QDir(msys).exists()) {
        const QString prefix = environment.value(QStringLiteral("CMAKE_PREFIX_PATH"));
        environment.insert(QStringLiteral("CMAKE_PREFIX_PATH"),
                           prefix.isEmpty() ? msys : msys + QStringLiteral(";") + prefix);
        const QString path = environment.value(QStringLiteral("PATH"));
        environment.insert(QStringLiteral("PATH"), msys + QStringLiteral("/bin;") + path);
    }
#endif

    const QString buildDir = QDir(project.rootPath).filePath(QStringLiteral("build"));
    if (buildProcess) {
        buildProcess->deleteLater();
        buildProcess = nullptr;
    }
    buildProcess = new QProcess(this);
    buildProcess->setProcessEnvironment(environment);
    buildProcess->setWorkingDirectory(project.rootPath);

    connect(buildProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        appendBuildOutput(QString::fromLocal8Bit(buildProcess->readAllStandardOutput()));
    });
    connect(buildProcess, &QProcess::readyReadStandardError, this, [this]() {
        appendBuildOutput(QString::fromLocal8Bit(buildProcess->readAllStandardError()));
    });
    connect(buildProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            appendBuildOutput(QStringLiteral("Falha ao iniciar o processo de build."));
        }
    });
    connect(buildProcess, &QProcess::finished, this,
            [this, cmake, buildDir](int exitCode, QProcess::ExitStatus status) {
                const bool ok = status == QProcess::NormalExit && exitCode == 0;
                if (buildCancelled) {
                    finishBuild(false);
                    return;
                }
                appendBuildOutput(ok ? QStringLiteral("[OK]")
                                     : QStringLiteral("[Falha: código %1]").arg(exitCode));
                if (buildPhase == 1 && ok) {
                    buildPhase = 2;
                    appendBuildOutput(QStringLiteral("> cmake --build %1").arg(buildDir));
                    buildProcess->start(cmake, {QStringLiteral("--build"), buildDir});
                    if (!buildProcess->waitForStarted(3000)) {
                        appendBuildOutput(QStringLiteral("Falha ao iniciar cmake --build."));
                        finishBuild(false);
                    }
                    return;
                }
                finishBuild(ok);
            });

    buildCancelled = false;
    buildPhase = 1;
    buildView->clear();
    outputDock->show();
    outputTabs->setCurrentWidget(buildView);
    appendBuildOutput(QStringLiteral("> cmake -S %1 -B %2 -G Ninja").arg(project.rootPath, buildDir));
    buildProcess->start(cmake, {QStringLiteral("-S"), project.rootPath, QStringLiteral("-B"),
                                buildDir, QStringLiteral("-G"), QStringLiteral("Ninja")});
    if (!buildProcess->waitForStarted(3000)) {
        appendBuildOutput(QStringLiteral("Falha ao iniciar cmake."));
        finishBuild(false);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Compilando... (F7 novamente ou Shift+F7 cancela)"));
}

void MainWindow::finishBuild(bool success)
{
    const bool cancelled = buildCancelled;
    const bool shouldRun = runAfterBuild && success && !cancelled;
    buildCancelled = false;
    runAfterBuild = false;
    buildPhase = 0;

    if (!cancelled) {
        appendBuildOutput(success ? QStringLiteral("Compilação concluída.")
                                  : QStringLiteral("Compilação falhou."));
        statusBar()->showMessage(success ? QStringLiteral("Compilação concluída.")
                                         : QStringLiteral("Compilação falhou."), 4000);
    } else {
        statusBar()->showMessage(QStringLiteral("Compilação cancelada."), 3000);
    }

    if (success && !cancelled && !buildHadCompileCommands && lsp) {
        const QString compileCommands = QDir(project.rootPath)
                                            .filePath(QStringLiteral("build/compile_commands.json"));
        if (QFileInfo::exists(compileCommands)) {
            appendOutput(QStringLiteral("compile_commands.json detectado; reiniciando clangd."));
            lsp->start(project.rootPath);
        }
    }
    if (shouldRun) {
        runProject();
    }
}

void MainWindow::cancelBuild()
{
    if (!buildProcess || buildProcess->state() == QProcess::NotRunning) {
        return;
    }
    buildCancelled = true;
    appendBuildOutput(QStringLiteral("[cancelando...]"));
    buildProcess->kill();
}

void MainWindow::runProject()
{
    if (project.rootPath.isEmpty()) {
        appendOutput(QStringLiteral("Abra ou crie um projeto antes de executar."));
        return;
    }

    if (runProcess && runProcess->state() != QProcess::NotRunning) {
        stopRun();
    }

    const QString target = project.name.trimmed().replace(QLatin1Char(' '), QLatin1Char('_'));
    const QString executable = QDir(project.rootPath)
                                   .filePath(QStringLiteral("build/%1.exe").arg(target));

    if (!QFileInfo::exists(executable)) {
        appendOutput(QStringLiteral("Executável não encontrado. Compilando antes de executar..."));
        runAfterBuild = true;
        if (!buildProcess || buildProcess->state() == QProcess::NotRunning) {
            buildProject();
        }
        return;
    }

    terminalView->clear();
    outputTabs->setCurrentWidget(terminalView);
    const QString workingDirectory = QFileInfo(executable).absolutePath();
    appendTerminal(QStringLiteral("> %1").arg(executable));

    runProcess = new QProcess(this);
    runProcess->setWorkingDirectory(workingDirectory);
    connect(runProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        appendTerminal(QString::fromLocal8Bit(runProcess->readAllStandardOutput()));
    });
    connect(runProcess, &QProcess::readyReadStandardError, this, [this]() {
        appendTerminal(QString::fromLocal8Bit(runProcess->readAllStandardError()));
    });
    connect(runProcess, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        appendTerminal(status == QProcess::NormalExit
                           ? QStringLiteral("[processo terminou com código %1]").arg(exitCode)
                           : QStringLiteral("[processo foi encerrado]"));
        statusBar()->showMessage(QStringLiteral("Execução finalizada."), 3000);
    });
    runProcess->start(executable, {});
    if (!runProcess->waitForStarted(5000)) {
        appendTerminal(QStringLiteral("Falha ao iniciar: %1").arg(runProcess->errorString()));
        return;
    }
    statusBar()->showMessage(QStringLiteral("Aplicação iniciada (Shift+F5 para parar)."), 3000);
}

void MainWindow::stopRun()
{
    if (!runProcess || runProcess->state() == QProcess::NotRunning) {
        return;
    }
    appendTerminal(QStringLiteral("[encerrando...]"));
    runProcess->terminate();
    if (!runProcess->waitForFinished(1500)) {
        runProcess->kill();
        runProcess->waitForFinished(1000);
    }
}

void MainWindow::requestCompletion()
{
    auto *editor = qobject_cast<CodeEditor *>(sender());
    if (!editor) {
        editor = qobject_cast<CodeEditor *>(editorTabs->currentWidget());
    }
    if (!editor || !lsp || !lsp->isInitialized()) {
        statusBar()->showMessage(QStringLiteral("clangd ainda não está pronto."), 2000);
        return;
    }
    const QTextCursor cursor = editor->textCursor();
    QPointer<CodeEditor> target = editor;
    lsp->requestCompletion(editor->filePath(), cursor.blockNumber(), cursor.positionInBlock(),
                           [target](const QVector<LspCompletionItem> &items) {
                               if (target) {
                                   target->showCompletions(items);
                               }
                           });
}

void MainWindow::requestDefinition()
{
    auto *editor = qobject_cast<CodeEditor *>(sender());
    if (!editor) {
        editor = qobject_cast<CodeEditor *>(editorTabs->currentWidget());
    }
    if (!editor || !lsp || !lsp->isInitialized()) {
        return;
    }
    const QTextCursor cursor = editor->textCursor();
    lsp->requestDefinition(editor->filePath(), cursor.blockNumber(), cursor.positionInBlock(),
                           [this](const LspLocation &location) {
                               if (location.filePath.isEmpty()) {
                                   statusBar()->showMessage(QStringLiteral("Definição não encontrada."), 2000);
                                   return;
                               }
                               openDocument(location.filePath);
                               auto *target = qobject_cast<CodeEditor *>(editorTabs->currentWidget());
                               if (!target) {
                                   return;
                               }
                               QTextBlock block = target->document()->findBlockByNumber(location.line);
                               if (!block.isValid()) {
                                   return;
                               }
                               QTextCursor cursor(block);
                               cursor.setPosition(block.position()
                                                  + qMin(location.character, block.length() - 1));
                               target->setTextCursor(cursor);
                               target->setFocus();
                           });
}

void MainWindow::requestHover()
{
    auto *editor = qobject_cast<CodeEditor *>(sender());
    if (!editor) {
        editor = qobject_cast<CodeEditor *>(editorTabs->currentWidget());
    }
    if (!editor || !lsp || !lsp->isInitialized()) {
        return;
    }
    const QPoint position = editor->mapFromGlobal(QCursor::pos());
    const QTextCursor cursor = editor->cursorForPosition(position);
    QPointer<CodeEditor> target = editor;
    lsp->requestHover(editor->filePath(), cursor.blockNumber(), cursor.positionInBlock(),
                      [target](const QString &text) {
                          if (target && !text.isEmpty()) {
                              const QString shortened = text.length() > 1200
                                                            ? text.left(1200) + QStringLiteral("...")
                                                            : text;
                              QToolTip::showText(QCursor::pos(), shortened, target);
                          }
                      });
}

void MainWindow::onDiagnostics(const QString &filePath, const QVector<LspDiagnostic> &diagnostics)
{
    for (int i = 0; i < editorTabs->count(); ++i) {
        auto *editor = qobject_cast<CodeEditor *>(editorTabs->widget(i));
        if (editor && editor->filePath() == filePath) {
            editor->setDiagnostics(diagnostics);
            break;
        }
    }
    if (!diagnostics.isEmpty()) {
        int errors = 0;
        for (const LspDiagnostic &diagnostic : diagnostics) {
            if (diagnostic.severity <= 1) {
                ++errors;
            }
        }
        statusBar()->showMessage(QStringLiteral("%1: %2 problema(s), %3 erro(s)")
                                     .arg(QFileInfo(filePath).fileName())
                                     .arg(diagnostics.size())
                                     .arg(errors),
                                 4000);
    }
}

void MainWindow::reopenDocumentsInLsp()
{
    if (!lsp) {
        return;
    }
    for (int i = 0; i < editorTabs->count(); ++i) {
        auto *editor = qobject_cast<CodeEditor *>(editorTabs->widget(i));
        if (!editor || editor->filePath().isEmpty() || lsp->hasFile(editor->filePath())) {
            continue;
        }
        lsp->openFile(editor->filePath(), editor->toPlainText());
    }
}

void MainWindow::toggleDarkTheme(bool enabled)
{
    QPalette palette = QApplication::palette();
    if (enabled) {
        palette.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
        palette.setColor(QPalette::WindowText, QColor(0xdd, 0xdd, 0xdd));
        palette.setColor(QPalette::Base, QColor(0x28, 0x28, 0x28));
        palette.setColor(QPalette::AlternateBase, QColor(0x33, 0x33, 0x33));
        palette.setColor(QPalette::Text, QColor(0xdd, 0xdd, 0xdd));
        palette.setColor(QPalette::Button, QColor(0x3a, 0x3a, 0x3a));
        palette.setColor(QPalette::ButtonText, QColor(0xdd, 0xdd, 0xdd));
        palette.setColor(QPalette::Highlight, QColor(0x2d, 0x7d, 0x9a));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::Mid, QColor(0x88, 0x88, 0x88));
    } else {
        palette = QApplication::style()->standardPalette();
    }
    QApplication::setPalette(palette);
}

void MainWindow::syncUiWatcher()
{
    if (!uiWatcher) {
        uiWatcher = new QFileSystemWatcher(this);
        connect(uiWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onUiFileChanged);
    }
    QStringList paths;
    if (!project.rootPath.isEmpty()) {
        for (const FormModel &form : project.forms) {
            paths << QDir(project.rootPath).filePath(QStringLiteral("forms/%1.ui").arg(form.name));
        }
    }
    for (const QString &old : uiWatcher->files()) {
        if (!paths.contains(old)) {
            uiWatcher->removePath(old);
            uiHashes.remove(old);
        }
    }
    for (const QString &path : paths) {
        if (!uiWatcher->files().contains(path) && QFileInfo::exists(path)) {
            uiWatcher->addPath(path);
        }
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            uiHashes.insert(path, QCryptographicHash::hash(file.readAll(),
                                                           QCryptographicHash::Sha256));
        }
    }
}

void MainWindow::onUiFileChanged(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QByteArray content = file.readAll();
    const QByteArray hash = QCryptographicHash::hash(content, QCryptographicHash::Sha256);
    if (uiHashes.value(path) == hash) {
        return;
    }
    uiHashes.insert(path, hash);
    if (!uiWatcher->files().contains(path)) {
        uiWatcher->addPath(path);
    }
    reloadUiFromDisk(path);
}

void MainWindow::reloadUiFromDisk(const QString &path)
{
    QString error;
    const Project loaded = Project::load(project.projectFilePath(), &error);
    if (!error.isEmpty()) {
        appendOutput(QStringLiteral("Falha ao recarregar interface: %1").arg(error));
        return;
    }
    project = loaded;
    refreshProjectTree();

    for (int i = 0; i < editorTabs->count(); ++i) {
        const QString tabPath = editorTabs->tabBar()->tabData(i).toString();
        if (tabPath != path) {
            continue;
        }
        auto *editor = qobject_cast<CodeEditor *>(editorTabs->widget(i));
        QFile file(path);
        if (editor && file.open(QIODevice::ReadOnly)) {
            editor->setPlainText(QString::fromUtf8(file.readAll()));
            editor->document()->setModified(false);
        }
    }

    if (designer && !project.forms.isEmpty()) {
        designer->setForm(project.forms.first());
    }
    appendOutput(QStringLiteral("Interface recarregada do Qt Designer: %1")
                     .arg(QFileInfo(path).fileName()));
    regenerateCode();
}

void MainWindow::editInterface()
{
    if (project.rootPath.isEmpty() || project.forms.isEmpty()) {
        appendOutput(QStringLiteral("Abra ou crie um projeto antes de editar a interface."));
        return;
    }
    const QString path = QDir(project.rootPath)
                             .filePath(QStringLiteral("forms/%1.ui").arg(project.forms.first().name));
    if (!QFileInfo::exists(path)) {
        QString error;
        if (!project.save(&error)) {
            appendOutput(error);
            return;
        }
        syncUiWatcher();
    }

    QString designer = QDir(QCoreApplication::applicationDirPath())
                           .filePath(QStringLiteral("designer.exe"));
    if (!QFileInfo::exists(designer)) {
        designer = findExecutable(QStringLiteral("designer"));
    }
#ifdef Q_OS_WIN
    if (designer.isEmpty() || !QFileInfo::exists(designer)) {
        const QString fallback = QStringLiteral("C:/msys64/mingw64/bin/designer.exe");
        if (QFileInfo::exists(fallback)) {
            designer = fallback;
        }
    }
#endif
    if (designer.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Qt Designer"),
                             QStringLiteral("Qt Designer não encontrado.\n"
                                            "Instale com: pacman -S mingw-w64-x86_64-qt6-tools"));
        return;
    }
    if (!QProcess::startDetached(designer, {path}, QFileInfo(designer).absolutePath())) {
        appendOutput(QStringLiteral("Falha ao iniciar o Qt Designer."));
        return;
    }
    appendOutput(QStringLiteral("Qt Designer aberto: %1").arg(path));
    statusBar()->showMessage(QStringLiteral("Editando interface no Qt Designer..."), 4000);
}
