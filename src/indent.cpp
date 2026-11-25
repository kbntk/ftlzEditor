#include "indent.h"

#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QtGlobal>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <initializer_list>

namespace {


struct Tag {
    int pos;
    int end;
    bool closing;
    bool mid;     // <#else>, <#elseif>, <#case>, <#default>, <#recover>
    bool opener;  // starts a block that is closed by a matching closing tag
};

// <#name attrs>, </#name>, <@name attrs>, </@name>, <#name attrs/>
const QRegularExpression &angleTag()
{
    static const QRegularExpression re(QStringLiteral(R"(<(/?)([#@])([A-Za-z_][\w.]*)([^>]*)>)"));
    return re;
}

// [#name attrs], [/#name], [@name attrs], [/@name]
const QRegularExpression &bracketTag()
{
    static const QRegularExpression re(QStringLiteral(R"(\[(/?)([#@])([A-Za-z_][\w.]*)([^\[\]]*)\])"));
    return re;
}

const QRegularExpression &commentRe()
{
    static const QRegularExpression re(QStringLiteral(R"(<#--.*?-->)"));
    return re;
}

const QSet<QString> &openers()
{
    static const QSet<QString> s{QStringLiteral("if"),       QStringLiteral("list"),     QStringLiteral("macro"),
                                 QStringLiteral("function"), QStringLiteral("switch"),   QStringLiteral("attempt"),
                                 QStringLiteral("compress"), QStringLiteral("escape"),   QStringLiteral("noescape"),
                                 QStringLiteral("noparse"),  QStringLiteral("items"),    QStringLiteral("foreach"),
                                 QStringLiteral("transform"), QStringLiteral("nested")};
    return s;
}

const QSet<QString> &mids()
{
    static const QSet<QString> s{QStringLiteral("else"), QStringLiteral("elseif"), QStringLiteral("case"),
                                 QStringLiteral("default"), QStringLiteral("recover")};
    return s;
}

// <#assign x>...</#assign> is a block; <#assign x = 1> is not.
const QSet<QString> &blockAssigns()
{
    static const QSet<QString> s{QStringLiteral("assign"), QStringLiteral("local"), QStringLiteral("global")};
    return s;
}

// <tag attrs>, </tag>, <tag/> with an HTML element name
const QRegularExpression &htmlTag()
{
    static const QRegularExpression re(QStringLiteral(R"(<(/?)([A-Za-z][A-Za-z0-9-]*)([^>]*)>)"));
    return re;
}

// HTML elements whose content is indented. Void elements (meta, br, img, link, ...)
// and inline elements do not change the depth, so they are not listed.
const QSet<QString> &htmlBlocks()
{
    static const QSet<QString> s{
        QStringLiteral("html"),  QStringLiteral("head"),    QStringLiteral("body"),  QStringLiteral("div"),
        QStringLiteral("section"), QStringLiteral("article"), QStringLiteral("header"), QStringLiteral("footer"),
        QStringLiteral("main"),  QStringLiteral("nav"),     QStringLiteral("aside"), QStringLiteral("table"),
        QStringLiteral("thead"), QStringLiteral("tbody"),   QStringLiteral("tfoot"), QStringLiteral("tr"),
        QStringLiteral("td"),    QStringLiteral("th"),      QStringLiteral("ul"),    QStringLiteral("ol"),
        QStringLiteral("dl"),    QStringLiteral("form"),    QStringLiteral("style"), QStringLiteral("script"),
        QStringLiteral("select"), QStringLiteral("figure"), QStringLiteral("blockquote")};
    return s;
}

QVector<Tag> scanTags(const QString &code)
{
    QVector<Tag> tags;
    for (const QRegularExpression *re : {&angleTag(), &bracketTag()}) {
        auto it = re->globalMatch(code);
        while (it.hasNext()) {
            const auto m = it.next();
            const bool closing = m.captured(1) == QLatin1String("/");
            const bool directive = m.captured(2) == QLatin1String("#");
            const QString name = m.captured(3);
            const QString attrs = m.captured(4);
            const bool selfClosing = attrs.trimmed().endsWith(QLatin1Char('/'));

            Tag t{static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedEnd()), closing, false, false};
            if (closing) {
                // closer, nothing else to decide
            } else if (directive && mids().contains(name)) {
                t.mid = true;
            } else if (selfClosing) {
                // <#include "x"/> and similar: no block
            } else if (!directive) {
                t.opener = true; // user directive <@macro>
            } else if (openers().contains(name)) {
                t.opener = true;
            } else if (blockAssigns().contains(name) && !attrs.contains(QLatin1Char('='))) {
                t.opener = true;
            }
            if (closing || t.opener || t.mid)
                tags.append(t);
        }
    }
    auto html = htmlTag().globalMatch(code);
    while (html.hasNext()) {
        const auto m = html.next();
        const QString name = m.captured(2).toLower();
        if (!htmlBlocks().contains(name))
            continue;
        const bool closing = m.captured(1) == QLatin1String("/");
        const bool selfClosing = m.captured(3).trimmed().endsWith(QLatin1Char('/'));
        Tag t{static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedEnd()), closing, false,
              !closing && !selfClosing};
        tags.append(t);
    }

    std::sort(tags.begin(), tags.end(), [](const Tag &a, const Tag &b) { return a.pos < b.pos; });
    return tags;
}

} // namespace

QString IndentSettings::unit() const
{
    return useTabs ? QStringLiteral("\t") : QString(qBound(1, spaces, 9), QLatin1Char(' '));
}

IndentSettings loadIndentSettings()
{
    QSettings settings;
    IndentSettings s;
    s.useTabs = settings.value(QStringLiteral("indentUseTabs"), false).toBool();
    s.spaces = qBound(1, settings.value(QStringLiteral("indentSpaces"), 4).toInt(), 9);
    return s;
}

void saveIndentSettings(const IndentSettings &s)
{
    QSettings settings;
    settings.setValue(QStringLiteral("indentUseTabs"), s.useTabs);
    settings.setValue(QStringLiteral("indentSpaces"), qBound(1, s.spaces, 9));
}

QString autoIndentFtl(const QString &text, const QString &indentUnit)
{
    QStringList out;
    int depth = 0;
    bool inComment = false;

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        QString raw = rawLine;
        if (raw.endsWith(QLatin1Char('\r')))
            raw.chop(1);

        if (inComment) {
            out << raw; // comment bodies are not touched
            if (raw.contains(QLatin1String("-->")))
                inComment = false;
            continue;
        }

        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty()) {
            out << QString();
            continue;
        }

        QString code = trimmed;
        code.remove(commentRe());
        const QVector<Tag> tags = scanTags(code);

        // Leading closing tags (and one leading mid tag) dedent this line.
        int dedent = 0;
        int expected = 0;
        for (const Tag &t : tags) {
            if (t.pos != expected)
                break;
            if (t.closing) {
                ++dedent;
            } else if (t.mid) {
                ++dedent;
                break;
            } else {
                break;
            }
            expected = t.end;
        }

        const int lineDepth = std::max(0, depth - dedent);
        QString indent;
        for (int i = 0; i < lineDepth; ++i)
            indent += indentUnit;
        out << indent + trimmed;

        for (const Tag &t : tags) {
            if (t.opener)
                ++depth;
            else if (t.closing)
                depth = std::max(0, depth - 1);
        }

        if (code.contains(QLatin1String("<#--")))
            inComment = true; // comment opened on this line and not closed
    }
    return out.join(QLatin1Char('\n'));
}
