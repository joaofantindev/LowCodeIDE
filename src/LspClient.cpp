#include "LspClient.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QUrl>

namespace {

QString toUri(const QString &filePath)
{
    return QUrl::fromLocalFile(QFileInfo(filePath).absoluteFilePath())
        .toString(QUrl::FullyEncoded);
}

QString fromUri(const QString &uri)
{
    return QUrl(uri).toLocalFile();
}

} // namespace

LspClient::LspClient(QObject *parent)
    : QObject(parent)
{
}

LspClient::~LspClient()
{
    if (process && process->state() != QProcess::NotRunning) {
        sendNotification(QStringLiteral("shutdown"), QJsonObject());
        sendNotification(QStringLiteral("exit"), QJsonObject());
        process->waitForFinished(1500);
        process->kill();
        process->waitForFinished(500);
    }
}

QString LspClient::findClangd()
{
    const QString found = QStandardPaths::findExecutable(QStringLiteral("clangd"));
    if (!found.isEmpty()) {
        return found;
    }
    const QStringList candidates{
        QStringLiteral("C:/msys64/mingw64/bin/clangd.exe"),
        QStringLiteral("C:/Program Files/LLVM/bin/clangd.exe"),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

bool LspClient::isRunning() const
{
    return process && process->state() == QProcess::Running;
}

bool LspClient::isInitialized() const
{
    return initialized;
}

bool LspClient::hasFile(const QString &filePath) const
{
    return versions.contains(filePath);
}

void LspClient::start(const QString &workspace)
{
    workspacePath = QFileInfo(workspace).absoluteFilePath();
    const QString clangd = findClangd();
    if (clangd.isEmpty()) {
        emit message(QStringLiteral("clangd não encontrado (instale mingw-w64-x86_64-clang-tools-extra)."));
        emit started(false);
        return;
    }

    if (process) {
        process->kill();
        process->waitForFinished(1000);
        delete process;
    }
    buffer.clear();
    initialized = false;
    versions.clear();
    pending.clear();

    process = new QProcess(this);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    QStringList arguments;
    const QString compileCommands = QDir(workspacePath).filePath(QStringLiteral("build/compile_commands.json"));
    if (QFileInfo::exists(compileCommands)) {
        arguments << QStringLiteral("--compile-commands-dir=%1").arg(QDir(workspacePath).filePath(QStringLiteral("build")));
    }
    arguments << QStringLiteral("--background-index") << QStringLiteral("--log=error");

    connect(process, &QProcess::readyReadStandardOutput, this, &LspClient::readStandardOutput);
    connect(process, &QProcess::readyReadStandardError, this, &LspClient::readStandardError);
    connect(process, &QProcess::finished, this, &LspClient::processFinished);
    process->start(clangd, arguments);
    if (!process->waitForStarted(5000)) {
        emit message(QStringLiteral("Falha ao iniciar clangd: %1").arg(process->errorString()));
        emit started(false);
        return;
    }

    QJsonObject rootUri;
    const QString uri = toUri(workspacePath);
    QJsonObject workspaceFolder;
    workspaceFolder.insert(QStringLiteral("uri"), uri);
    workspaceFolder.insert(QStringLiteral("name"), QDir(workspacePath).dirName());

    QJsonObject params;
    params.insert(QStringLiteral("processId"), static_cast<qint64>(QCoreApplication::applicationPid()));
    params.insert(QStringLiteral("rootUri"), uri);
    params.insert(QStringLiteral("capabilities"), QJsonObject());
    params.insert(QStringLiteral("workspaceFolders"), QJsonArray{workspaceFolder});

    sendRequest(QStringLiteral("initialize"), params, [this](const QJsonValue &result) {
        if (!result.isObject()) {
            emit message(QStringLiteral("clangd rejeitou initialize."));
            emit started(false);
            return;
        }
        initialized = true;
        sendNotification(QStringLiteral("initialized"), QJsonObject());
        sendNotification(QStringLiteral("workspace/didChangeConfiguration"),
                         QJsonObject{{QStringLiteral("settings"), QJsonObject()}});
        emit message(QStringLiteral("clangd pronto."));
        emit started(true);
    });
}

QJsonObject LspClient::textDocument(const QString &filePath) const
{
    QJsonObject document;
    document.insert(QStringLiteral("uri"), toUri(filePath));
    return document;
}

void LspClient::openFile(const QString &filePath, const QString &text)
{
    if (!initialized) {
        return;
    }
    versions.insert(filePath, 1);
    QJsonObject document = textDocument(filePath);
    document.insert(QStringLiteral("languageId"), QStringLiteral("cpp"));
    document.insert(QStringLiteral("version"), 1);
    document.insert(QStringLiteral("text"), text);
    sendNotification(QStringLiteral("textDocument/didOpen"),
                     QJsonObject{{QStringLiteral("textDocument"), document}});
}

void LspClient::changeFile(const QString &filePath, const QString &text)
{
    if (!initialized) {
        return;
    }
    const int version = versions.value(filePath, 1) + 1;
    versions.insert(filePath, version);
    QJsonObject document = textDocument(filePath);
    document.insert(QStringLiteral("version"), version);
    QJsonObject change;
    change.insert(QStringLiteral("text"), text);
    QJsonObject params;
    params.insert(QStringLiteral("textDocument"), document);
    params.insert(QStringLiteral("contentChanges"), QJsonArray{change});
    sendNotification(QStringLiteral("textDocument/didChange"), params);
}

void LspClient::closeFile(const QString &filePath)
{
    if (!initialized) {
        return;
    }
    sendNotification(QStringLiteral("textDocument/didClose"),
                     QJsonObject{{QStringLiteral("textDocument"), textDocument(filePath)}});
    versions.remove(filePath);
}

void LspClient::requestCompletion(const QString &filePath, int line, int character,
                                  const std::function<void(const QVector<LspCompletionItem> &)> &callback)
{
    if (!initialized) {
        callback({});
        return;
    }
    QJsonObject params;
    params.insert(QStringLiteral("textDocument"), textDocument(filePath));
    QJsonObject position;
    position.insert(QStringLiteral("line"), line);
    position.insert(QStringLiteral("character"), character);
    params.insert(QStringLiteral("position"), position);
    sendRequest(QStringLiteral("textDocument/completion"), params,
                [callback](const QJsonValue &result) {
                    QVector<LspCompletionItem> items;
                    QJsonArray array;
                    if (result.isArray()) {
                        array = result.toArray();
                    } else if (result.isObject()) {
                        array = result.toObject().value(QStringLiteral("items")).toArray();
                    }
                    for (const QJsonValue &value : array) {
                        const QJsonObject object = value.toObject();
                        LspCompletionItem item;
                        item.label = object.value(QStringLiteral("label")).toString();
                        item.insertText = object.value(QStringLiteral("insertText")).toString(item.label);
                        item.detail = object.value(QStringLiteral("detail")).toString();
                        if (!item.label.isEmpty()) {
                            items.append(item);
                        }
                    }
                    callback(items);
                });
}

void LspClient::requestHover(const QString &filePath, int line, int character,
                             const std::function<void(const QString &)> &callback)
{
    if (!initialized) {
        callback(QString());
        return;
    }
    QJsonObject params;
    params.insert(QStringLiteral("textDocument"), textDocument(filePath));
    QJsonObject position;
    position.insert(QStringLiteral("line"), line);
    position.insert(QStringLiteral("character"), character);
    params.insert(QStringLiteral("position"), position);
    sendRequest(QStringLiteral("textDocument/hover"), params, [callback](const QJsonValue &result) {
        if (!result.isObject()) {
            callback(QString());
            return;
        }
        const QJsonValue contents = result.toObject().value(QStringLiteral("contents"));
        if (contents.isString()) {
            callback(contents.toString());
        } else if (contents.isObject()) {
            callback(contents.toObject().value(QStringLiteral("value")).toString());
        } else if (contents.isArray()) {
            QStringList parts;
            for (const QJsonValue &part : contents.toArray()) {
                if (part.isString()) {
                    parts.append(part.toString());
                } else if (part.isObject()) {
                    parts.append(part.toObject().value(QStringLiteral("value")).toString());
                }
            }
            callback(parts.join(QStringLiteral("\n")));
        } else {
            callback(QString());
        }
    });
}

void LspClient::requestDefinition(const QString &filePath, int line, int character,
                                  const std::function<void(const LspLocation &)> &callback)
{
    if (!initialized) {
        callback(LspLocation());
        return;
    }
    QJsonObject params;
    params.insert(QStringLiteral("textDocument"), textDocument(filePath));
    QJsonObject position;
    position.insert(QStringLiteral("line"), line);
    position.insert(QStringLiteral("character"), character);
    params.insert(QStringLiteral("position"), position);
    sendRequest(QStringLiteral("textDocument/definition"), params,
                [callback](const QJsonValue &result) {
                    QJsonObject locationObject;
                    if (result.isArray() && !result.toArray().isEmpty()) {
                        locationObject = result.toArray().first().toObject();
                    } else if (result.isObject()) {
                        locationObject = result.toObject();
                    }
                    if (locationObject.isEmpty()) {
                        callback(LspLocation());
                        return;
                    }
                    LspLocation location;
                    location.filePath = fromUri(locationObject.value(QStringLiteral("uri")).toString());
                    const QJsonObject range = locationObject.value(QStringLiteral("range")).toObject();
                    const QJsonObject start = range.value(QStringLiteral("start")).toObject();
                    location.line = start.value(QStringLiteral("line")).toInt(0);
                    location.character = start.value(QStringLiteral("character")).toInt(0);
                    callback(location);
                });
}

void LspClient::sendMessage(const QByteArray &payload)
{
    if (!process || process->state() != QProcess::Running) {
        return;
    }
    QByteArray message = "Content-Length: " + QByteArray::number(payload.size()) + "\r\n\r\n";
    message.append(payload);
    process->write(message);
}

void LspClient::sendRequest(const QString &method, const QJsonObject &params,
                            const std::function<void(const QJsonValue &)> &callback)
{
    const int id = nextId++;
    pending.insert(id, callback);
    QJsonObject request;
    request.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    request.insert(QStringLiteral("id"), id);
    request.insert(QStringLiteral("method"), method);
    request.insert(QStringLiteral("params"), params);
    sendMessage(QJsonDocument(request).toJson(QJsonDocument::Compact));
}

void LspClient::sendNotification(const QString &method, const QJsonObject &params)
{
    QJsonObject notification;
    notification.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    notification.insert(QStringLiteral("method"), method);
    notification.insert(QStringLiteral("params"), params);
    sendMessage(QJsonDocument(notification).toJson(QJsonDocument::Compact));
}

void LspClient::readStandardOutput()
{
    buffer.append(process->readAllStandardOutput());
    while (true) {
        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            break;
        }
        int length = -1;
        const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
        for (const QByteArray &line : lines) {
            const QByteArray trimmed = line.trimmed();
            if (trimmed.startsWith("Content-Length:")) {
                length = trimmed.mid(15).trimmed().toInt();
                break;
            }
        }
        if (length < 0) {
            buffer.remove(0, headerEnd + 4);
            continue;
        }
        const int payloadStart = headerEnd + 4;
        if (buffer.size() < payloadStart + length) {
            break;
        }
        const QByteArray payload = buffer.mid(payloadStart, length);
        buffer.remove(0, payloadStart + length);

        const QJsonDocument document = QJsonDocument::fromJson(payload);
        if (document.isObject()) {
            handleMessage(document.object());
        }
    }
}

void LspClient::readStandardError()
{
    const QString text = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
    if (!text.isEmpty()) {
        emit message(text);
    }
}

void LspClient::processFinished()
{
    initialized = false;
    emit message(QStringLiteral("clangd encerrado."));
}

void LspClient::handleMessage(const QJsonObject &message)
{
    if (message.contains(QStringLiteral("id")) && message.contains(QStringLiteral("result"))) {
        handleResponse(message);
    } else if (message.contains(QStringLiteral("method"))) {
        handleNotification(message);
    }
}

void LspClient::handleResponse(const QJsonObject &message)
{
    const int id = message.value(QStringLiteral("id")).toInt();
    const auto callback = pending.take(id);
    if (callback) {
        callback(message.value(QStringLiteral("result")));
    }
}

void LspClient::handleNotification(const QJsonObject &notification)
{
    const QString method = notification.value(QStringLiteral("method")).toString();
    if (method == QStringLiteral("textDocument/publishDiagnostics")) {
        handleDiagnostics(notification.value(QStringLiteral("params")).toObject());
    } else if (method == QStringLiteral("window/logMessage")) {
        const QString text = notification.value(QStringLiteral("params")).toObject()
                                 .value(QStringLiteral("message")).toString();
        if (!text.isEmpty()) {
            emit message(text);
        }
    }
}

void LspClient::handleDiagnostics(const QJsonObject &params)
{
    const QString filePath = fromUri(params.value(QStringLiteral("uri")).toString());
    QVector<LspDiagnostic> diagnostics;
    for (const QJsonValue &value : params.value(QStringLiteral("diagnostics")).toArray()) {
        const QJsonObject object = value.toObject();
        LspDiagnostic diagnostic;
        diagnostic.severity = object.value(QStringLiteral("severity")).toInt(0);
        diagnostic.message = object.value(QStringLiteral("message")).toString();
        const QJsonObject range = object.value(QStringLiteral("range")).toObject();
        const QJsonObject start = range.value(QStringLiteral("start")).toObject();
        const QJsonObject end = range.value(QStringLiteral("end")).toObject();
        diagnostic.line = start.value(QStringLiteral("line")).toInt(0);
        diagnostic.character = start.value(QStringLiteral("character")).toInt(0);
        diagnostic.endLine = end.value(QStringLiteral("line")).toInt(diagnostic.line);
        diagnostic.endCharacter = end.value(QStringLiteral("character")).toInt(diagnostic.character);
        diagnostics.append(diagnostic);
    }
    emit diagnosticsReady(filePath, diagnostics);
}
