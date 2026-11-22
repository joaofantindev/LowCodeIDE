#include "SyntaxHighlighter.h"

#include <QColor>
#include <QStringList>

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor("#c678dd"));
    const QStringList keywords{
        "alignas", "auto", "bool", "break", "case", "class", "const", "constexpr",
        "continue", "default", "delete", "do", "double", "else", "enum", "explicit",
        "false", "float", "for", "friend", "if", "inline", "int", "long", "namespace",
        "new", "nullptr", "operator", "private", "protected", "public", "return",
        "short", "signed", "sizeof", "static", "struct", "switch", "template", "this",
        "throw", "true", "try", "typedef", "typename", "unsigned", "using", "virtual",
        "void", "while", "override", "final", "Q_OBJECT", "signals", "slots"
    };
    for (const QString &word : keywords) {
        HighlightRule rule;
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(word));
        rule.format = keywordFormat;
        rules.append(rule);
    }

    QTextCharFormat classFormat;
    classFormat.setForeground(QColor("#61afef"));
    HighlightRule classRule;
    classRule.pattern = QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b"));
    classRule.format = classFormat;
    rules.append(classRule);

    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor("#98c379"));
    HighlightRule stringRule;
    stringRule.pattern = QRegularExpression(QStringLiteral("\".*\""));
    stringRule.format = stringFormat;
    rules.append(stringRule);

    QTextCharFormat preprocessorFormat;
    preprocessorFormat.setForeground(QColor("#e5c07b"));
    HighlightRule preprocessorRule;
    preprocessorRule.pattern = QRegularExpression(QStringLiteral("^\\s*#.*$"));
    preprocessorRule.format = preprocessorFormat;
    rules.append(preprocessorRule);

    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor("#d19a66"));
    HighlightRule numberRule;
    numberRule.pattern = QRegularExpression(QStringLiteral("\\b\\d+(\\.\\d+)?\\b"));
    numberRule.format = numberFormat;
    rules.append(numberRule);

    QTextCharFormat singleLineCommentFormat;
    singleLineCommentFormat.setForeground(QColor("#7f848e"));
    HighlightRule commentRule;
    commentRule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
    commentRule.format = singleLineCommentFormat;
    rules.append(commentRule);

    commentFormat.setForeground(QColor("#7f848e"));
    commentStartExpression = QRegularExpression(QStringLiteral("/\\*"));
    commentEndExpression = QRegularExpression(QStringLiteral("\\*/"));
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightRule &rule : rules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            const QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1) {
        startIndex = text.indexOf(commentStartExpression);
    }
    while (startIndex >= 0) {
        const QRegularExpressionMatch endMatch = commentEndExpression.match(text, startIndex);
        int endIndex = endMatch.capturedStart();
        int commentLength = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, commentLength, commentFormat);
        startIndex = text.indexOf(commentStartExpression, startIndex + commentLength);
    }
}
