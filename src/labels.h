#pragma once

#include <QString>
#include <QStringList>

// Locale-aware ordering of labels.
bool labelLess(const QString &a, const QString &b);

// Suggestions for a partially typed label: labels that START with the query,
// or in which a WORD starts with the query (words are split on space, '_', '-', '.').
// "Słu" suggests "UM Słupca" but not "UG Strzałkowo". Exact matches are skipped.
QStringList suggestLabels(const QString &query, const QStringList &labels, int maxCount = 10);

// Case-insensitive substring filter; an empty filter matches everything.
bool labelMatchesFilter(const QString &label, const QString &filter);
