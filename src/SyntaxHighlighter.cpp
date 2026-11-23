#include "SyntaxHighlighter.h"

#include <QColor>
#include <QTextBlock>
#include <QStringList>

bool SyntaxHighlighter::darkTheme = false;
QVector<QPointer<SyntaxHighlighter>> SyntaxHighlighter::instances;

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    instances.append(this);
    rebuildRules();
}

SyntaxHighlighter::~SyntaxHighlighter()
{
    instances.removeAll(this);
}

void SyntaxHighlighter::setDarkTheme(bool dark)
{
    if (darkTheme == dark) {
        return;
    }
    darkTheme = dark;
    for (const QPointer<SyntaxHighlighter> &highlighter : instances) {
        if (highlighter) {
            highlighter->rebuildRules();
            highlighter->rehighlight();
        }
    }
}

bool SyntaxHighlighter::isDarkTheme()
{
    return darkTheme;
}

void SyntaxHighlighter::rebuildRules()
{
    rules.clear();

    // Mesma paleta dos arquivos themes/*.qss.
    const QString keywordColor = darkTheme ? QStringLiteral("#F5AE24") : QStringLiteral("#E99A17");
    const QString classColor = QStringLiteral("#477CB8");
    const QString stringColor = QStringLiteral("#78B94A");
    const QString preprocessorColor = QStringLiteral("#E98228");
    const QString numberColor = darkTheme ? QStringLiteral("#E98228") : QStringLiteral("#79513A");
    const QString commentColor = darkTheme ? QStringLiteral("#737887") : QStringLiteral("#71695F");

    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(keywordColor));
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
    classFormat.setForeground(QColor(classColor));
    HighlightRule classRule;
    classRule.pattern = QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b"));
    classRule.format = classFormat;
    rules.append(classRule);

    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(stringColor));
    HighlightRule stringRule;
    stringRule.pattern = QRegularExpression(QStringLiteral("\".*\""));
    stringRule.format = stringFormat;
    rules.append(stringRule);

    QTextCharFormat preprocessorFormat;
    preprocessorFormat.setForeground(QColor(preprocessorColor));
    HighlightRule preprocessorRule;
    preprocessorRule.pattern = QRegularExpression(QStringLiteral("^\\s*#.*$"));
    preprocessorRule.format = preprocessorFormat;
    rules.append(preprocessorRule);

    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(numberColor));
    HighlightRule numberRule;
    numberRule.pattern = QRegularExpression(QStringLiteral("\\b\\d+(\\.\\d+)?\\b"));
    numberRule.format = numberFormat;
    rules.append(numberRule);

    QTextCharFormat singleLineCommentFormat;
    singleLineCommentFormat.setForeground(QColor(commentColor));
    singleLineCommentFormat.setFontItalic(true);
    HighlightRule commentRule;
    commentRule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
    commentRule.format = singleLineCommentFormat;
    rules.append(commentRule);

    commentFormat.setForeground(QColor(commentColor));
    commentFormat.setFontItalic(true);
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
