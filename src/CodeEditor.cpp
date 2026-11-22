#include "CodeEditor.h"

#include <QAbstractItemView>
#include <QColor>
#include <QCompleter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>
#include <QWidget>

namespace {
class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditor *editor)
        : QWidget(editor), editor(editor)
    {
    }

    QSize sizeHint() const override
    {
        return QSize(editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        editor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor *editor = nullptr;
};
} // namespace

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);

    completionModel = new QStringListModel(this);
    completer = new QCompleter(this);
    completer->setModel(completionModel);
    completer->setWidget(this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    connect(completer, static_cast<void (QCompleter:: *)(const QString &)>(&QCompleter::activated),
            this, &CodeEditor::insertCompletion);

    hoverTimer = new QTimer(this);
    hoverTimer->setSingleShot(true);
    hoverTimer->setInterval(450);
    connect(hoverTimer, &QTimer::timeout, this, &CodeEditor::emitHoverRequest);

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);
    connect(this, &QPlainTextEdit::cursorPositionChanged, hoverTimer, &QTimer::stop);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

void CodeEditor::setFilePath(const QString &filePath)
{
    currentFilePath = filePath;
}

QString CodeEditor::filePath() const
{
    return currentFilePath;
}

void CodeEditor::showCompletions(const QVector<LspCompletionItem> &items)
{
    if (items.isEmpty()) {
        completer->popup()->hide();
        return;
    }

    QStringList labels;
    completionInsertTexts.clear();
    for (const LspCompletionItem &item : items) {
        labels.append(item.label);
        completionInsertTexts.insert(item.label, item.insertText);
    }
    completionModel->setStringList(labels);

    const QTextCursor cursor = textCursor();
    const int start = wordStartPosition(cursor);
    QTextCursor prefixCursor = cursor;
    prefixCursor.setPosition(start);
    prefixCursor.setPosition(cursor.position(), QTextCursor::KeepAnchor);
    completer->setCompletionPrefix(prefixCursor.selectedText());
    completer->popup()->setCurrentIndex(completionModel->index(0, 0));
    completer->complete();
}

void CodeEditor::setDiagnostics(const QVector<LspDiagnostic> &diagnostics)
{
    this->diagnostics = diagnostics;
    highlightCurrentLine();
}

QString CodeEditor::leadingWhitespace(const QString &line)
{
    int count = 0;
    while (count < line.size() && (line.at(count) == QLatin1Char(' ') || line.at(count) == QLatin1Char('\t'))) {
        ++count;
    }
    return line.left(count);
}

int CodeEditor::wordStartPosition(const QTextCursor &cursor) const
{
    int position = cursor.position();
    while (position > 0) {
        const QChar character = document()->characterAt(position - 1);
        if (character.isLetterOrNumber() || character == QLatin1Char('_') || character == QLatin1Char(':')
            || character == QLatin1Char('>') || character == QLatin1Char('~')) {
            --position;
        } else {
            break;
        }
    }
    return position;
}

void CodeEditor::insertCompletion(const QString &completion)
{
    QString insertText = completionInsertTexts.value(completion, completion);
    insertText.replace(QRegularExpression(QStringLiteral("\\$\\{\\d+:([^}]*)\\}")), QStringLiteral("\\1"));
    insertText.remove(QRegularExpression(QStringLiteral("\\$\\d+")));

    QTextCursor cursor = textCursor();
    const int start = wordStartPosition(cursor);
    cursor.setPosition(start);
    cursor.setPosition(textCursor().position(), QTextCursor::KeepAnchor);
    cursor.insertText(insertText);
    setTextCursor(cursor);
}

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && event->modifiers() == Qt::ControlModifier) {
        emit completionRequested();
        return;
    }

    if (completer->popup()->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:
            break;
        }
    }

    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && event->modifiers() == Qt::NoModifier && !textCursor().hasSelection()) {
        const QString line = textCursor().block().text();
        const QString trimmed = line.trimmed();
        int indent = leadingWhitespace(line).size();
        if (trimmed.endsWith(QLatin1Char('{')) || trimmed.endsWith(QLatin1Char(':'))
            || trimmed.endsWith(QLatin1Char('('))) {
            indent += 4;
        }
        if (trimmed.startsWith(QLatin1Char('}'))) {
            indent = qMax(0, indent - 4);
        }
        insertPlainText(QStringLiteral("\n") + QString(indent, QLatin1Char(' ')));
        return;
    }

    const QString typed = event->text();
    if (typed.size() == 1 && !textCursor().hasSelection()
        && event->modifiers() == Qt::NoModifier) {
        const QChar character = typed.at(0);
        const QChar closing = character == QLatin1Char('(') ? QLatin1Char(')')
                              : character == QLatin1Char('[') ? QLatin1Char(']')
                              : character == QLatin1Char('{') ? QLatin1Char('}')
                              : character == QLatin1Char('"') ? QLatin1Char('"')
                              : QChar();
        if (!closing.isNull()) {
            insertPlainText(typed + closing);
            QTextCursor cursor = textCursor();
            cursor.movePosition(QTextCursor::Left);
            setTextCursor(cursor);
            return;
        }
        if (character == QLatin1Char(')') || character == QLatin1Char(']')
            || character == QLatin1Char('}') || character == QLatin1Char('"')) {
            if (document()->characterAt(textCursor().position()) == character) {
                QTextCursor cursor = textCursor();
                cursor.movePosition(QTextCursor::Right);
                setTextCursor(cursor);
                return;
            }
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}

void CodeEditor::mouseMoveEvent(QMouseEvent *event)
{
    QPlainTextEdit::mouseMoveEvent(event);
    if (event->buttons() == Qt::NoButton && !completer->popup()->isVisible()) {
        hoverTimer->start();
    }
}

void CodeEditor::emitHoverRequest()
{
    hoverTimer->stop();
    emit hoverRequested();
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    const int space = 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect rect = contentsRect();
    lineNumberArea->setGeometry(QRect(rect.left(), rect.top(), lineNumberAreaWidth(), rect.height()));
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy != 0) {
        lineNumberArea->scroll(0, dy);
    } else {
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
    }
    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(palette().color(QPalette::AlternateBase));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }

    for (const LspDiagnostic &diagnostic : diagnostics) {
        QTextBlock block = document()->findBlockByNumber(diagnostic.line);
        if (!block.isValid()) {
            continue;
        }
        const int start = block.position() + qMin(diagnostic.character, block.length() - 1);
        int end = diagnostic.endLine == diagnostic.line ? diagnostic.endCharacter : block.length() - 1;
        end = qMin(qMax(end, diagnostic.character + 1), block.length() - 1);
        if (end <= start) {
            continue;
        }
        QTextCursor cursor(block);
        cursor.setPosition(start);
        cursor.setPosition(block.position() + end, QTextCursor::KeepAnchor);
        QTextEdit::ExtraSelection selection;
        selection.cursor = cursor;
        selection.format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
        selection.format.setUnderlineColor(QColor(QStringLiteral("#e06c75")));
        selection.format.setToolTip(diagnostic.message);
        selections.append(selection);
    }

    setExtraSelections(selections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), palette().color(QPalette::Window));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const QString number = QString::number(blockNumber + 1);
            painter.setPen(palette().color(QPalette::Mid));
            painter.drawText(0, top, lineNumberArea->width() - 6, fontMetrics().height(),
                             Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}
