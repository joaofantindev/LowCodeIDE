#pragma once

#include "LspClient.h"

#include <QHash>
#include <QPlainTextEdit>
#include <QString>

class QCompleter;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QStringListModel;
class QTimer;
class QWidget;

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void setFilePath(const QString &filePath);
    QString filePath() const;

    void showCompletions(const QVector<LspCompletionItem> &items);
    void setDiagnostics(const QVector<LspDiagnostic> &diagnostics);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth() const;

signals:
    void completionRequested();
    void definitionRequested();
    void hoverRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);
    void insertCompletion(const QString &completion);
    void emitHoverRequest();

private:
    int wordStartPosition(const QTextCursor &cursor) const;
    static QString leadingWhitespace(const QString &line);

    QWidget *lineNumberArea = nullptr;
    QCompleter *completer = nullptr;
    QStringListModel *completionModel = nullptr;
    QHash<QString, QString> completionInsertTexts;
    QVector<LspDiagnostic> diagnostics;
    QString currentFilePath;
    QTimer *hoverTimer = nullptr;
};
