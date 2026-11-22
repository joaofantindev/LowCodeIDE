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
#include <QFont>
#include <QInputDialog>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
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
    outputView = new QPlainTextEdit(outputDock);
    outputView->setReadOnly(true);
    outputView->setMaximumBlockCount(2000);
    outputDock->setWidget(outputView);
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

    QAction *generateAction = projectMenu->addAction(QStringLiteral("&Gerar código"));
    generateAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    connect(generateAction, &QAction::triggered, this, &MainWindow::regenerateCode);
    projectToolbar->addAction(generateAction);

    QAction *buildAction = projectMenu->addAction(QStringLiteral("&Compilar"));
    buildAction->setShortcut(QKeySequence(QStringLiteral("F7")));
    connect(buildAction, &QAction::triggered, this, &MainWindow::buildProject);
    projectToolbar->addAction(buildAction);

    QAction *runAction = projectMenu->addAction(QStringLiteral("&Executar"));
    runAction->setShortcut(QKeySequence(QStringLiteral("F5")));
    connect(runAction, &QAction::triggered, this, &MainWindow::runProject);
    projectToolbar->addAction(runAction);

    statusBar()->showMessage(QStringLiteral("Pronto. Crie ou abra um projeto para começar."));
}

void MainWindow::appendOutput(const QString &text)
{
    outputView->appendPlainText(text);
    outputDock->show();
    outputDock->raise();
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

    const QStringList initialFiles{
        QStringLiteral("src/main.cpp"),
        QStringLiteral("src/MainWindow.cpp"),
        QStringLiteral("src/MainWindow.h"),
        QStringLiteral("generated/ui_mainwindow.h"),
        QStringLiteral("generated/app_vars.h"),
        QStringLiteral("project.json"),
    };
    for (const QString &relative : initialFiles) {
        const QString path = QDir(project.rootPath).filePath(relative);
        if (QFileInfo::exists(path)) {
            openDocument(path);
        }
    }
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
    editor->setFont(QFont(QStringLiteral("Consolas"), 10));
    editor->setPlainText(QString::fromUtf8(content));
    editor->document()->setModified(false);
    new SyntaxHighlighter(editor->document());

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
    if (project.rootPath.isEmpty()) {
        appendOutput(QStringLiteral("Abra ou crie um projeto antes de compilar."));
        return;
    }

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
    auto run = [&](const QString &program, const QStringList &arguments) {
        appendOutput(QStringLiteral("> %1 %2").arg(program, arguments.join(QLatin1Char(' '))));
        QProcess process(this);
        process.setProcessEnvironment(environment);
        process.setWorkingDirectory(project.rootPath);
        process.start(program, arguments);
        if (!process.waitForStarted(10000)) {
            appendOutput(QStringLiteral("Falha ao iniciar %1.").arg(program));
            return false;
        }
        process.waitForFinished(300000);
        const QString standardOutput = QString::fromLocal8Bit(process.readAllStandardOutput());
        const QString standardError = QString::fromLocal8Bit(process.readAllStandardError());
        if (!standardOutput.trimmed().isEmpty()) {
            appendOutput(standardOutput.trimmed());
        }
        if (!standardError.trimmed().isEmpty()) {
            appendOutput(standardError.trimmed());
        }
        const bool success = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        appendOutput(success ? QStringLiteral("OK")
                             : QStringLiteral("Falha (código %1).").arg(process.exitCode()));
        return success;
    };

    if (!run(cmake, {QStringLiteral("-S"), project.rootPath, QStringLiteral("-B"), buildDir,
                     QStringLiteral("-G"), QStringLiteral("Ninja")})) {
        return;
    }
    run(cmake, {QStringLiteral("--build"), buildDir});
}

void MainWindow::runProject()
{
    if (project.rootPath.isEmpty()) {
        appendOutput(QStringLiteral("Abra ou crie um projeto antes de executar."));
        return;
    }

    const QString target = project.name.trimmed().replace(QLatin1Char(' '), QLatin1Char('_'));
    const QString executable = QDir(project.rootPath)
                                   .filePath(QStringLiteral("build/%1.exe").arg(target));

    if (!QFileInfo::exists(executable)) {
        appendOutput(QStringLiteral("Executável não encontrado. Compilando antes de executar..."));
        buildProject();
    }
    if (!QFileInfo::exists(executable)) {
        appendOutput(QStringLiteral("Não foi possível compilar o projeto. Verifique a saída."));
        return;
    }

    const QString workingDirectory = QFileInfo(executable).absolutePath();
    appendOutput(QStringLiteral("Executando %1").arg(executable));
    if (!QProcess::startDetached(executable, {}, workingDirectory)) {
        appendOutput(QStringLiteral("Falha ao iniciar %1.").arg(executable));
        return;
    }
    statusBar()->showMessage(QStringLiteral("Aplicação iniciada."), 3000);
}
