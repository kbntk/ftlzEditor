#include "labels.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>

bool labelLess(const QString &a, const QString &b)
{
    return QString::localeAwareCompare(a, b) < 0;
}

QStringList suggestLabels(const QString &query, const QStringList &labels, int maxCount)
{
    const QString q = query.trimmed().toLower();
    if (q.isEmpty())
        return {};

    static const QRegularExpression separators(QStringLiteral("[\\s_\\-.]+"));

    QStringList startsWith;
    QStringList wordStartsWith;
    QSet<QString> seen;
    for (const QString &label : labels) {
        if (seen.contains(label))
            continue;
        seen.insert(label);

        const QString l = label.toLower();
        if (l == q)
            continue;
        if (l.startsWith(q)) {
            startsWith << label;
            continue;
        }
        const QStringList words = l.split(separators, Qt::SkipEmptyParts);
        for (const QString &w : words)
            if (w.startsWith(q)) {
                wordStartsWith << label;
                break;
            }
    }

    std::sort(startsWith.begin(), startsWith.end(), labelLess);
    std::sort(wordStartsWith.begin(), wordStartsWith.end(), labelLess);

    QStringList result = startsWith + wordStartsWith;
    if (result.size() > maxCount)
        result.resize(maxCount);
    return result;
}

bool labelMatchesFilter(const QString &label, const QString &filter)
{
    const QString f = filter.trimmed();
    return f.isEmpty() || label.contains(f, Qt::CaseInsensitive);
}
