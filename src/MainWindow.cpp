#include "MainWindow.h"

#include "CodeEditor.h"
#include "CodeGenerator.h"
#include "FormDesigner.h"
#include "SyntaxHighlighter.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFrame>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
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
#include <QSize>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#ifndef Q_OS_WIN
#include <signal.h>
#include <unistd.h>
#endif

namespace {
QIcon iconForPath(const QString &path, bool isDir)
{
    if (isDir) {
        return QIcon(QStringLiteral(":/images/folder.png"));
    }
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("cpp") || suffix == QStringLiteral("h")
        || suffix == QStringLiteral("hpp") || suffix == QStringLiteral("cc")) {
        return QIcon(QStringLiteral(":/images/file_cpp.png"));
    }
    if (suffix == QStringLiteral("json") || suffix == QStringLiteral("ui")
        || suffix == QStringLiteral("qss") || suffix == QStringLiteral("qrc")
        || suffix == QStringLiteral("md")) {
        return QIcon(QStringLiteral(":/images/file_config.png"));
    }
    if (suffix == QStringLiteral("exe") || suffix == QStringLiteral("ps1")
        || suffix == QStringLiteral("bat")) {
        return QIcon(QStringLiteral(":/images/file_run.png"));
    }
    return QIcon(QStringLiteral(":/images/file_text.png"));
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    scratchDir = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                     .filePath(QStringLiteral("CppIDE-Testes"));
    QDir().mkpath(scratchDir);
    setupUi();
    refreshProjectTree();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("C++ Visual IDE"));
    setWindowIcon(QIcon(QStringLiteral(":/images/mascot.png")));
    resize(1250, 820);

    editorTabs = new QTabWidget(this);
    editorTabs->setTabsClosable(true);
    connect(editorTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    centralStack = new QStackedWidget(this);
    buildWelcomePage();
    centralStack->addWidget(editorTabs);
    setCentralWidget(centralStack);

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
    buildView->setObjectName(QStringLiteral("buildView"));
    buildView->setReadOnly(true);
    buildView->setMaximumBlockCount(5000);
    buildView->setFont(QFont(QStringLiteral("Consolas"), 10));
    outputTabs->addTab(outputView, QStringLiteral("Saída"));
    outputTabs->setTabIcon(0, QIcon(QStringLiteral(":/images/icon_plugins.png")));
    outputTabs->addTab(terminalView, QStringLiteral("Terminal"));
    outputTabs->setTabIcon(1, QIcon(QStringLiteral(":/images/icon_terminal.png")));
    outputTabs->addTab(buildView, QStringLiteral("Build"));
    outputTabs->setTabIcon(2, QIcon(QStringLiteral(":/images/icon_debug.png")));
    outputDock->setWidget(outputTabs);
    addDockWidget(Qt::BottomDockWidgetArea, outputDock);

    QToolBar *projectToolbar = addToolBar(QStringLiteral("Projeto"));
    projectToolbar->setObjectName(QStringLiteral("projectToolbar"));
    projectToolbar->setIconSize(QSize(26, 26));

    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&Arquivo"));

    QAction *newAction = fileMenu->addAction(QStringLiteral("&Novo Projeto..."));
    newAction->setIcon(QIcon(QStringLiteral(":/images/folders_new.png")));
    newAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")));
    connect(newAction, &QAction::triggered, this, &MainWindow::newProject);

    QAction *openAction = fileMenu->addAction(QStringLiteral("&Abrir Projeto..."));
    openAction->setIcon(QIcon(QStringLiteral(":/images/folders_open.png")));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openProject);

    QAction *saveAction = fileMenu->addAction(QStringLiteral("&Salvar"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveProject);

    fileMenu->addSeparator();
    QAction *newCppAction = fileMenu->addAction(QStringLiteral("&Novo arquivo C++ para teste..."));
    newCppAction->setIcon(QIcon(QStringLiteral(":/images/file_cpp.png")));
    newCppAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+N")));
    connect(newCppAction, &QAction::triggered, this, &MainWindow::newScratchFile);
    projectToolbar->addAction(newCppAction);

    QAction *openCppAction = fileMenu->addAction(QStringLiteral("A&brir arquivo C++..."));
    openCppAction->setIcon(QIcon(QStringLiteral(":/images/file_text.png")));
    connect(openCppAction, &QAction::triggered, this, &MainWindow::openCppFile);

    QAction *runCppAction = fileMenu->addAction(QStringLiteral("Compilar e executar &arquivo (teste)"));
    runCppAction->setIcon(QIcon(QStringLiteral(":/images/file_run.png")));
    runCppAction->setShortcut(QKeySequence(QStringLiteral("F6")));
    connect(runCppAction, &QAction::triggered, this, &MainWindow::runScratchFile);
    projectToolbar->addAction(runCppAction);

    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(QStringLiteral("S&air"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *projectMenu = menuBar()->addMenu(QStringLiteral("&Projeto"));

    QAction *designerAction = projectMenu->addAction(QStringLiteral("&Editar interface (Qt Designer)"));
    designerAction->setIcon(QIcon(QStringLiteral(":/images/icon_cursor.png")));
    designerAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    connect(designerAction, &QAction::triggered, this, &MainWindow::editInterface);
    projectToolbar->addAction(designerAction);

    QAction *generateAction = projectMenu->addAction(QStringLiteral("&Gerar código"));
    generateAction->setIcon(QIcon(QStringLiteral(":/images/icon_tools.png")));
    generateAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    connect(generateAction, &QAction::triggered, this, &MainWindow::regenerateCode);
    projectToolbar->addAction(generateAction);

    QAction *buildAction = projectMenu->addAction(QStringLiteral("&Compilar"));
    buildAction->setIcon(QIcon(QStringLiteral(":/images/icon_debug.png")));
    buildAction->setShortcut(QKeySequence(QStringLiteral("F7")));
    connect(buildAction, &QAction::triggered, this, &MainWindow::buildProject);
    projectToolbar->addAction(buildAction);

    QAction *cancelBuildAction = projectMenu->addAction(QStringLiteral("Cancelar &build"));
    cancelBuildAction->setIcon(QIcon(QStringLiteral(":/images/icon_bug.png")));
    cancelBuildAction->setShortcut(QKeySequence(QStringLiteral("Shift+F7")));
    connect(cancelBuildAction, &QAction::triggered, this, &MainWindow::cancelBuild);

    QAction *runAction = projectMenu->addAction(QStringLiteral("&Executar"));
    runAction->setIcon(QIcon(QStringLiteral(":/images/icon_play.png")));
    runAction->setShortcut(QKeySequence(QStringLiteral("F5")));
    connect(runAction, &QAction::triggered, this, &MainWindow::runProject);
    projectToolbar->addAction(runAction);

    QAction *stopAction = projectMenu->addAction(QStringLiteral("&Parar execução"));
    stopAction->setIcon(QIcon(QStringLiteral(":/images/icon_stop.png")));
    stopAction->setShortcut(QKeySequence(QStringLiteral("Shift+F5")));
    connect(stopAction, &QAction::triggered, this, &MainWindow::stopRun);
    projectToolbar->addAction(stopAction);

    QMenu *codeMenu = menuBar()->addMenu(QIcon(QStringLiteral(":/images/icon_plugins.png")),
                                         QStringLiteral("&Código"));
    QAction *completeAction = codeMenu->addAction(QStringLiteral("&Autocomplete"));
    completeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Space")));
    connect(completeAction, &QAction::triggered, this, &MainWindow::requestCompletion);

    QAction *definitionAction = codeMenu->addAction(QStringLiteral("&Ir para definição"));
    definitionAction->setShortcut(QKeySequence(QStringLiteral("F2")));
    connect(definitionAction, &QAction::triggered, this, &MainWindow::requestDefinition);

    statusBar()->showMessage(QStringLiteral("Pronto. Crie um projeto ou um arquivo C++ de teste."));
    applyTheme();
}

void MainWindow::buildWelcomePage()
{
    welcomePage = new QFrame(centralStack);
    welcomePage->setObjectName(QStringLiteral("welcomePage"));
    auto *layout = new QVBoxLayout(welcomePage);
    layout->setContentsMargins(36, 24, 36, 24);
    layout->setSpacing(8);
    layout->addStretch(2);

    auto *logoLabel = new QLabel(welcomePage);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setPixmap(QPixmap(QStringLiteral(":/images/logo.png"))
                             .scaled(240, 141, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(logoLabel);

    auto *titleLabel = new QLabel(QStringLiteral("C++ Visual IDE"), welcomePage);
    titleLabel->setObjectName(QStringLiteral("welcomeTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel(QStringLiteral("RAD em Qt Widgets — leve, rápido e seu."),
                                     welcomePage);
    subtitleLabel->setObjectName(QStringLiteral("welcomeSubtitle"));
    subtitleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitleLabel);
    layout->addSpacing(20);

    auto *card = new QFrame(welcomePage);
    card->setObjectName(QStringLiteral("welcomeCard"));
    card->setMaximumWidth(600);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 18, 24, 18);
    cardLayout->setSpacing(6);
    auto *hints = new QLabel(
        QStringLiteral("<b>Comece por aqui</b><br>"
                       "• Arquivo › Novo arquivo C++ para teste (Ctrl+N) — compila com F6<br>"
                       "• Arquivo › Novo Projeto... — criar um projeto<br>"
                       "• Projeto › Editar interface (Ctrl+D) — Qt Designer<br>"
                       "• F7 compila sem travar · F5 executa no terminal do sistema<br>"
                       "• Ctrl+Space autocomplete · F2 vai para a definição"),
        card);
    hints->setObjectName(QStringLiteral("welcomeHint"));
    hints->setWordWrap(true);
    hints->setTextInteractionFlags(Qt::NoTextInteraction);
    cardLayout->addWidget(hints);
    layout->addWidget(card, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    auto *mascotLabel = new QLabel(welcomePage);
    mascotLabel->setAlignment(Qt::AlignCenter);
    mascotLabel->setPixmap(QPixmap(QStringLiteral(":/images/mascot_laptop.png"))
                               .scaled(160, 141, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(mascotLabel);
    layout->addStretch(1);

    centralStack->addWidget(welcomePage);
}

void MainWindow::applyTheme()
{
    QFile styleFile(QStringLiteral(":/themes/dark.qss"));
    if (styleFile.open(QIODevice::ReadOnly)) {
        qApp->setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    QPalette palette = QApplication::style()->standardPalette();
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#242424")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#F2E6D5")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#242424")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#383838")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#F2E6D5")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#303030")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#F2E6D5")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#477CB8")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#F2E6D5")));
    palette.setColor(QPalette::Mid, QColor(QStringLiteral("#737887")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#383838")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#F2E6D5")));
    QApplication::setPalette(palette);
    SyntaxHighlighter::setDarkTheme(true);
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

void MainWindow::newScratchFile()
{
    bool ok = false;
    QString name = QInputDialog::getText(
        this, QStringLiteral("Novo arquivo C++ de teste"), QStringLiteral("Nome do arquivo:"),
        QLineEdit::Normal, QStringLiteral("teste"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    name = name.trimmed();
    for (QChar &character : name) {
        if (!character.isLetterOrNumber() && character != QLatin1Char('_')
            && character != QLatin1Char('-') && character != QLatin1Char('.')) {
            character = QLatin1Char('_');
        }
    }
    if (!name.endsWith(QStringLiteral(".cpp"), Qt::CaseInsensitive)) {
        name += QStringLiteral(".cpp");
    }

    const QString path = QDir(scratchDir).filePath(name);
    if (QFileInfo::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("Novo arquivo C++ de teste"),
                             QStringLiteral("O arquivo já existe:\n%1").arg(path));
        return;
    }

    const QString source = QStringLiteral(
        "#include <iostream>\n\n"
        "int main()\n"
        "{\n"
        "    std::cout << \"Olá do C++ Visual IDE!\" << std::endl;\n"
        "    return 0;\n"
        "}\n");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, QStringLiteral("Novo arquivo C++ de teste"), file.errorString());
        return;
    }
    file.write(source.toUtf8());
    if (!file.commit()) {
        QMessageBox::critical(this, QStringLiteral("Novo arquivo C++ de teste"), file.errorString());
        return;
    }
    centralStack->setCurrentWidget(editorTabs);
    refreshProjectTree();
    openDocument(path);
    statusBar()->showMessage(QStringLiteral("Arquivo de teste criado: %1").arg(path), 3000);
}

void MainWindow::openCppFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Abrir arquivo C++"), scratchDir,
        QStringLiteral("Arquivos C++ (*.cpp *.cc *.cxx *.h *.hpp);;Todos os arquivos (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    centralStack->setCurrentWidget(editorTabs);
    openDocument(path);
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
    centralStack->setCurrentWidget(editorTabs);
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
    if (!project.rootPath.isEmpty()) {
    auto *root = new QTreeWidgetItem(projectTree, {project.name.isEmpty() ? QStringLiteral("Projeto")
                                                                          : project.name});
    root->setData(0, Qt::UserRole, project.rootPath);
    root->setIcon(0, QIcon(QStringLiteral(":/images/folder_open.png")));
    root->setFont(0, QFont(QApplication::font().family(), QApplication::font().pointSize(), QFont::Bold));
    addTreeItems(root, project.rootPath);
    projectTree->addTopLevelItem(root);
    projectTree->expandItem(root);
    }

    auto *scratchRoot = new QTreeWidgetItem(projectTree,
                                            {QStringLiteral("Arquivos C++ soltos (testes)")});
    scratchRoot->setData(0, Qt::UserRole, scratchDir);
    scratchRoot->setIcon(0, QIcon(QStringLiteral(":/images/file_cpp.png")));
    scratchRoot->setFont(0, QFont(QApplication::font().family(), QApplication::font().pointSize(),
                                  QFont::Bold));
    addScratchItems(scratchRoot);
    projectTree->addTopLevelItem(scratchRoot);
    projectTree->expandItem(scratchRoot);
}

void MainWindow::addScratchItems(QTreeWidgetItem *parent)
{
    const QFileInfoList entries = QDir(scratchDir).entryInfoList(
        {QStringLiteral("*.cpp"), QStringLiteral("*.cc"), QStringLiteral("*.cxx"),
         QStringLiteral("*.h"), QStringLiteral("*.hpp")},
        QDir::Files, QDir::Name);
    for (const QFileInfo &info : entries) {
        auto *item = new QTreeWidgetItem(parent, {info.fileName()});
        item->setData(0, Qt::UserRole, info.absoluteFilePath());
        item->setIcon(0, iconForPath(info.absoluteFilePath(), false));
    }
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
        item->setIcon(0, iconForPath(info.absoluteFilePath(), info.isDir()));
        if (info.isDir()) {
            addTreeItems(item, info.absoluteFilePath());
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

QString MainWindow::currentSourcePath() const
{
    const int index = editorTabs->currentIndex();
    if (index < 0) {
        return {};
    }
    const QString path = editorTabs->tabBar()->tabData(index).toString();
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("cpp") || suffix == QStringLiteral("cc")
        || suffix == QStringLiteral("cxx") || suffix == QStringLiteral("c")) {
        return path;
    }
    return {};
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

qint64 MainWindow::launchInTerminal(const QString &executable, const QString &workingDirectory)
{
    qint64 pid = 0;
    bool started = false;
#ifdef Q_OS_WIN
    const QString cmd = findExecutable(QStringLiteral("cmd"));
    const QString program = cmd.isEmpty() ? QStringLiteral("cmd.exe") : cmd;
    started = QProcess::startDetached(
        program, {QStringLiteral("/D"), QStringLiteral("/K"), QDir::toNativeSeparators(executable)},
        workingDirectory, &pid);
#else
    const QStringList terminals{QStringLiteral("x-terminal-emulator"),
                                QStringLiteral("gnome-terminal"), QStringLiteral("konsole"),
                                QStringLiteral("xfce4-terminal"), QStringLiteral("xterm")};
    for (const QString &terminal : terminals) {
        QStringList args;
        if (terminal == QStringLiteral("gnome-terminal")) {
            args << QStringLiteral("--") << executable;
        } else {
            args << QStringLiteral("-e") << executable;
        }
        started = QProcess::startDetached(terminal, args, workingDirectory, &pid);
        if (started) {
            break;
        }
    }
#endif
    if (!started) {
        appendOutput(QStringLiteral("Não foi possível abrir o terminal do sistema."));
        return 0;
    }
    appendTerminal(QStringLiteral("> %1").arg(executable));
    statusBar()->showMessage(QStringLiteral("Executando no terminal do sistema (Shift+F5 para parar)."),
                             4000);
    return pid;
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
        runAfterBuild = true;
        if (!buildProcess || buildProcess->state() == QProcess::NotRunning) {
            buildProject();
        }
        return;
    }

    terminalView->clear();
    outputTabs->setCurrentWidget(terminalView);
    runPid = launchInTerminal(executable, QFileInfo(executable).absolutePath());
}

void MainWindow::stopRun()
{
    if (runPid <= 0) {
        statusBar()->showMessage(QStringLiteral("Nenhuma execução ativa."), 2000);
        return;
    }
#ifdef Q_OS_WIN
    QProcess::execute(QStringLiteral("taskkill"),
                      {QStringLiteral("/PID"), QString::number(runPid), QStringLiteral("/F")});
#else
    ::kill(static_cast<pid_t>(runPid), SIGTERM);
#endif
    appendTerminal(QStringLiteral("[processo encerrado]"));
    runPid = 0;
    statusBar()->showMessage(QStringLiteral("Execução encerrada."), 3000);
}

void MainWindow::runScratchFile()
{
    if (scratchProcess && scratchProcess->state() != QProcess::NotRunning) {
        appendBuildOutput(QStringLiteral("[já há uma compilação em andamento]"));
        return;
    }

    const QString source = currentSourcePath();
    if (source.isEmpty()) {
        appendOutput(QStringLiteral("Abra um arquivo .cpp para compilar e executar o teste (F6)."));
        return;
    }
    auto *editor = qobject_cast<CodeEditor *>(editorTabs->currentWidget());
    if (editor && editor->document()->isModified()) {
        saveProject();
    }

    const QString compiler = findExecutable(QStringLiteral("g++"));
    if (compiler.isEmpty()) {
        appendOutput(QStringLiteral("g++ não encontrado no PATH."));
        return;
    }

#ifdef Q_OS_WIN
    const QString executable = QDir(scratchDir)
                                   .filePath(QFileInfo(source).completeBaseName()
                                             + QStringLiteral(".exe"));
#else
    const QString executable = QDir(scratchDir).filePath(QFileInfo(source).completeBaseName());
#endif

    if (scratchProcess) {
        scratchProcess->deleteLater();
    }
    scratchProcess = new QProcess(this);
    connect(scratchProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        appendBuildOutput(QString::fromLocal8Bit(scratchProcess->readAllStandardOutput()));
    });
    connect(scratchProcess, &QProcess::readyReadStandardError, this, [this]() {
        appendBuildOutput(QString::fromLocal8Bit(scratchProcess->readAllStandardError()));
    });
    connect(scratchProcess, &QProcess::finished, this,
            [this, executable](int exitCode, QProcess::ExitStatus status) {
                const bool ok = status == QProcess::NormalExit && exitCode == 0;
                if (!ok) {
                    appendBuildOutput(QStringLiteral("[Falha na compilação: código %1]").arg(exitCode));
                    statusBar()->showMessage(QStringLiteral("Compilação do teste falhou."), 4000);
                    return;
                }
                appendBuildOutput(QStringLiteral("[OK] Executando no terminal do sistema..."));
                statusBar()->showMessage(QStringLiteral("Teste compilado."), 3000);
                runPid = launchInTerminal(executable, QFileInfo(executable).absolutePath());
            });

    buildView->clear();
    outputDock->show();
    outputTabs->setCurrentWidget(buildView);
    appendBuildOutput(QStringLiteral("> g++ -std=c++20 -Wall %1 -o %2").arg(source, executable));
    scratchProcess->start(compiler, {QStringLiteral("-std=c++20"), QStringLiteral("-Wall"), source,
                                     QStringLiteral("-o"), executable});
    if (!scratchProcess->waitForStarted(3000)) {
        appendBuildOutput(QStringLiteral("Falha ao iniciar g++."));
        statusBar()->showMessage(QStringLiteral("Falha ao iniciar g++."), 4000);
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
