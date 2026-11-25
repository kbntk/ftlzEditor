#include "ftlhighlighter.h"

namespace {
enum BlockState { Normal = 0, InComment = 1 };
}

FtlHighlighter::FtlHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
    auto makeFormat = [](const char *color, bool bold = false, bool italic = false) {
        QTextCharFormat f;
        f.setForeground(QColor(QLatin1String(color)));
        f.setFontWeight(bold ? QFont::Bold : QFont::Normal);
        f.setFontItalic(italic);
        return f;
    };

    // Order matters: later rules override earlier ones on overlapping text.
    // Directives: <#if ...>, </#list>, and user directives <@macro ...>, [#if ...], [@macro ...]
    m_rules.append({QRegularExpression(QStringLiteral("</?#[^>]*>")), makeFormat("#0055aa", true)});
    m_rules.append({QRegularExpression(QStringLiteral("</?@[^>]*>")), makeFormat("#7a3e9d", true)});
    m_rules.append({QRegularExpression(QStringLiteral("\\[/?[#@][^\\]]*\\]")), makeFormat("#0055aa", true)});
    // Interpolations: ${...} and #{...}
    m_rules.append({QRegularExpression(QStringLiteral("\\$\\{[^}]*\\}")), makeFormat("#b35c00")});
    m_rules.append({QRegularExpression(QStringLiteral("#\\{[^}]*\\}")), makeFormat("#b35c00")});
    // String literals
    m_rules.append({QRegularExpression(QStringLiteral("\"(?:[^\"\\\\]|\\\\.)*\"")), makeFormat("#2e7d32")});

    m_comment = makeFormat("#808080", false, true);
}

void FtlHighlighter::highlightBlock(const QString &text)
{
    for (const Rule &rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }

    // Comments <#-- ... --> may span several lines, so they are handled last
    // and carry their state to the next block.
    int pos = 0;
    if (previousBlockState() == InComment) {
        const int end = text.indexOf(QStringLiteral("-->"));
        if (end < 0) {
            setFormat(0, text.size(), m_comment);
            setCurrentBlockState(InComment);
            return;
        }
        setFormat(0, end + 3, m_comment);
        pos = end + 3;
    }

    while (true) {
        const int start = text.indexOf(QStringLiteral("<#--"), pos);
        if (start < 0)
            break;
        const int end = text.indexOf(QStringLiteral("-->"), start + 4);
        if (end < 0) {
            setFormat(start, text.size() - start, m_comment);
            setCurrentBlockState(InComment);
            return;
        }
        setFormat(start, end + 3 - start, m_comment);
        pos = end + 3;
    }

    setCurrentBlockState(Normal);
}
