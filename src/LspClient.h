#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

class QProcess;
class QJsonObject;
class QJsonValue;

struct LspCompletionItem {
    QString label;
    QString insertText;
    QString detail;
};

struct LspDiagnostic {
    int line = 0;
    int character = 0;
    int endLine = 0;
    int endCharacter = 0;
    int severity = 0;
    QString message;
};

struct LspLocation {
    QString filePath;
    int line = 0;
    int character = 0;
};

class LspClient : public QObject {
    Q_OBJECT

public:
    explicit LspClient(QObject *parent = nullptr);
    ~LspClient() override;

    static QString findClangd();

    void start(const QString &workspace);
    bool isRunning() const;
    bool isInitialized() const;
    bool hasFile(const QString &filePath) const;

    void openFile(const QString &filePath, const QString &text);
    void changeFile(const QString &filePath, const QString &text);
    void closeFile(const QString &filePath);

    void requestCompletion(const QString &filePath, int line, int character,
                           const std::function<void(const QVector<LspCompletionItem> &)> &callback);
    void requestHover(const QString &filePath, int line, int character,
                      const std::function<void(const QString &)> &callback);
    void requestDefinition(const QString &filePath, int line, int character,
                           const std::function<void(const LspLocation &)> &callback);

signals:
    void diagnosticsReady(const QString &filePath, const QVector<LspDiagnostic> &diagnostics);
    void message(const QString &text);
    void started(bool ok);

private slots:
    void readStandardOutput();
    void readStandardError();
    void processFinished();

private:
    void sendRequest(const QString &method, const QJsonObject &params,
                     const std::function<void(const QJsonValue &)> &callback);
    void sendNotification(const QString &method, const QJsonObject &params);
    void sendMessage(const QByteArray &payload);
    void handleMessage(const QJsonObject &message);
    void handleResponse(const QJsonObject &message);
    void handleNotification(const QJsonObject &message);
    void handleDiagnostics(const QJsonObject &params);
    QJsonObject textDocument(const QString &filePath) const;

    QProcess *process = nullptr;
    QByteArray buffer;
    int nextId = 1;
    QHash<int, std::function<void(const QJsonValue &)>> pending;
    QHash<QString, int> versions;
    QString workspacePath;
    bool initialized = false;
};
