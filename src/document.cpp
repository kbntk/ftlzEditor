#include "document.h"

#include "i18n.h"

#include <QFile>
#include <QJsonArray>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

#include "labels.h"

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// ---------------------------------------------------------------------------
// Lookup helpers
// ---------------------------------------------------------------------------

TextBlock *findBlock(Project &p, const QString &blockId)
{
    for (TextBlock &b : p.blocks)
        if (b.id == blockId)
            return &b;
    return nullptr;
}

int blockIndex(const Project &p, const QString &blockId)
{
    for (int i = 0; i < p.blocks.size(); ++i)
        if (p.blocks[i].id == blockId)
            return i;
    return -1;
}

FtlFile *findFile(TextBlock &b, const QString &fileId)
{
    for (FtlFile &f : b.files)
        if (f.id == fileId)
            return &f;
    return nullptr;
}

QStringList allFileLabels(const Project &p)
{
    QStringList labels;
    QSet<QString> seen;
    for (const TextBlock &b : p.blocks)
        for (const FtlFile &f : b.files) {
            const QString l = f.label.trimmed();
            if (!l.isEmpty() && !seen.contains(l)) {
                seen.insert(l);
                labels << l;
            }
        }
    std::sort(labels.begin(), labels.end(), labelLess);
    return labels;
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

namespace {

const char *viewToString(ViewMode v)
{
    switch (v) {
    case ViewMode::Minimized: return "minimized";
    case ViewMode::Maximized: return "maximized";
    case ViewMode::Normal: break;
    }
    return "normal";
}

ViewMode viewFromString(const QString &s)
{
    if (s == QLatin1String("minimized"))
        return ViewMode::Minimized;
    if (s == QLatin1String("maximized"))
        return ViewMode::Maximized;
    return ViewMode::Normal;
}

bool writeBytes(const QString &path, const QByteArray &data, QString &error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = f.errorString();
        return false;
    }
    if (f.write(data) != data.size()) {
        error = f.errorString();
        return false;
    }
    return true;
}

} // namespace

QJsonObject projectToJson(const Project &p)
{
    QJsonArray blocks;
    for (const TextBlock &b : p.blocks) {
        QJsonArray files;
        for (const FtlFile &f : b.files) {
            QJsonObject fo;
            fo.insert(QStringLiteral("id"), f.id);
            fo.insert(QStringLiteral("label"), f.label);
            fo.insert(QStringLiteral("path"), f.path);
            fo.insert(QStringLiteral("view"), QLatin1String(viewToString(f.view)));
            fo.insert(QStringLiteral("content"), f.content);
            files.append(fo);
        }
        QJsonObject bo;
        bo.insert(QStringLiteral("id"), b.id);
        bo.insert(QStringLiteral("label"), b.label);
        bo.insert(QStringLiteral("files"), files);
        blocks.append(bo);
    }

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("ftlz"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("collectivePrefix"), p.collectivePrefix);
    root.insert(QStringLiteral("blocks"), blocks);
    return root;
}

bool projectFromJson(const QJsonObject &root, Project &out, QString &error)
{
    if (root.value(QLatin1String("format")).toString() != QLatin1String("ftlz")) {
        error = tx("err.notFtlz");
        return false;
    }
    if (root.value(QLatin1String("version")).toInt() > 1) {
        error = tx("err.newerVersion");
        return false;
    }

    Project p;
    p.collectivePrefix = root.value(QLatin1String("collectivePrefix")).toString();

    const QJsonArray blocks = root.value(QLatin1String("blocks")).toArray();
    for (const QJsonValue &bv : blocks) {
        const QJsonObject bo = bv.toObject();
        TextBlock b;
        b.id = bo.value(QLatin1String("id")).toString();
        if (b.id.isEmpty())
            b.id = newId();
        b.label = bo.value(QLatin1String("label")).toString();

        for (const QJsonValue &fv : bo.value(QLatin1String("files")).toArray()) {
            const QJsonObject fo = fv.toObject();
            FtlFile f;
            f.id = fo.value(QLatin1String("id")).toString();
            if (f.id.isEmpty())
                f.id = newId();
            f.label = fo.value(QLatin1String("label")).toString();
            f.path = fo.value(QLatin1String("path")).toString();
            f.view = viewFromString(fo.value(QLatin1String("view")).toString());
            f.content = fo.value(QLatin1String("content")).toString();
            b.files.append(f);
        }
        if (b.files.isEmpty()) {
            FtlFile f;
            f.id = newId();
            f.label = b.label;
            b.files.append(f);
        }
        p.blocks.append(b);
    }

    out = p;
    return true;
}

bool readProjectFile(const QString &path, Project &out, QString &error)
{
    QString text;
    if (!readTextFile(path, text, error))
        return false;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (doc.isNull() || !doc.isObject()) {
        error = tx("err.parse").arg(parseError.errorString());
        return false;
    }
    if (!projectFromJson(doc.object(), out, error))
        return false;
    out.filePath = path;
    out.dirty = false;
    return true;
}

bool writeProjectFile(const Project &p, const QString &path, QString &error)
{
    const QByteArray data = QJsonDocument(projectToJson(p)).toJson(QJsonDocument::Indented);
    return writeBytes(path, data, error);
}

bool readTextFile(const QString &path, QString &out, QString &error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        error = f.errorString();
        return false;
    }
    out = QString::fromUtf8(f.readAll());
    if (out.startsWith(QChar(0xFEFF)))
        out.remove(0, 1); // strip UTF-8 BOM
    return true;
}

bool writeTextFile(const QString &path, const QString &content, QString &error)
{
    return writeBytes(path, content.toUtf8(), error);
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

QVector<MissingVariant> findMissingVariants(const Project &p)
{
    // All variant labels used by any variant block.
    QStringList all;
    for (const TextBlock &b : p.blocks) {
        if (b.isShared())
            continue;
        for (const FtlFile &f : b.files) {
            const QString l = f.label.trimmed();
            if (!l.isEmpty() && !all.contains(l))
                all << l;
        }
    }
    std::sort(all.begin(), all.end(), labelLess);

    QVector<MissingVariant> missing;
    for (const TextBlock &b : p.blocks) {
        if (b.isShared())
            continue;
        QStringList own;
        for (const FtlFile &f : b.files)
            own << f.label.trimmed();
        for (const QString &l : all)
            if (!own.contains(l))
                missing.append({b.label, l});
    }
    return missing;
}

QString sanitizeFileName(const QString &name)
{
    QString s = name.trimmed();
    s.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
    s.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
    return s;
}

GenerateResult generateCollective(const Project &p)
{
    GenerateResult r;

    // Variant labels, in order of first appearance.
    QStringList labels;
    for (const TextBlock &b : p.blocks) {
        if (b.isShared())
            continue;
        QSet<QString> inBlock;
        for (const FtlFile &f : b.files) {
            const QString label = f.label.trimmed();
            if (label.isEmpty()) {
                r.errors << tx("err.noLabel").arg(b.label);
                continue;
            }
            if (inBlock.contains(label))
                r.errors << tx("err.duplicateLabel").arg(label, b.label);
            inBlock.insert(label);
            if (!labels.contains(label))
                labels << label;
        }
    }
    if (labels.isEmpty() && r.errors.isEmpty())
        r.errors << tx("err.noVariants");
    if (!r.errors.isEmpty())
        return r;

    QSet<QString> names;
    for (const QString &label : labels) {
        GeneratedFile g;
        g.name = sanitizeFileName(p.collectivePrefix + label) + QStringLiteral(".ftl");
        if (names.contains(g.name)) {
            r.errors << tx("err.nameClash").arg(g.name);
            continue;
        }
        names.insert(g.name);

        for (const TextBlock &b : p.blocks) {
            const FtlFile *src = nullptr;
            if (b.isShared()) {
                if (!b.files.isEmpty())
                    src = &b.files.first();
            } else {
                for (const FtlFile &f : b.files)
                    if (f.label.trimmed() == label) {
                        src = &f;
                        break;
                    }
            }
            if (!src || src->content.isEmpty())
                continue;
            g.content += src->content;
            if (!g.content.endsWith(QLatin1Char('\n')))
                g.content += QLatin1Char('\n');
        }
        r.files << g;
    }
    return r;
}
