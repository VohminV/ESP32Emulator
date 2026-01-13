#ifndef CPPHIGHLIGHTER_H
#define CPPHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

class CppHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit CppHighlighter(QTextDocument *parent = nullptr) : QSyntaxHighlighter(parent) {
        // Ключевые слова
        QTextCharFormat keywordFormat;
        keywordFormat.setForeground(Qt::blue);
        keywordFormat.setFontWeight(QFont::Bold);

        const QStringList keywordPatterns = {
            "\\bint\\b", "\\bfloat\\b", "\\bdouble\\b", "\\bchar\\b",
            "\\bvoid\\b", "\\bbool\\b", "\\bif\\b", "\\belse\\b",
            "\\bfor\\b", "\\bwhile\\b", "\\breturn\\b"
        };

        for (const QString &pattern : keywordPatterns) {
            rules.append({QRegularExpression(pattern), keywordFormat});
        }

        // Комментарии
        QTextCharFormat commentFormat;
        commentFormat.setForeground(Qt::darkGreen);
        rules.append({QRegularExpression("//[^\n]*"), commentFormat});

        // Строки
        QTextCharFormat stringFormat;
        stringFormat.setForeground(Qt::red);
        rules.append({QRegularExpression("\".*\""), stringFormat});
    }

protected:
    void highlightBlock(const QString &text) override {
        for (const auto &rule : rules) {
            QRegularExpressionMatchIterator it = rule.first.globalMatch(text);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.second);
            }
        }
    }

private:
    QVector<QPair<QRegularExpression, QTextCharFormat>> rules;
};

#endif // CPPHIGHLIGHTER_H
