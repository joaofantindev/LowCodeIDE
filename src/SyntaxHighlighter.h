#pragma once

#include <QPointer>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit SyntaxHighlighter(QTextDocument *parent = nullptr);
    ~SyntaxHighlighter() override;

    static void setDarkTheme(bool dark);
    static bool isDarkTheme();

protected:
    void highlightBlock(const QString &text) override;

private:
    void rebuildRules();

    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightRule> rules;
    QTextCharFormat commentFormat;
    QRegularExpression commentStartExpression;
    QRegularExpression commentEndExpression;

    static bool darkTheme;
    static QVector<QPointer<SyntaxHighlighter>> instances;
};
