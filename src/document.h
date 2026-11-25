#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

// ---------------------------------------------------------------------------
// Model
//
// Project  -> blocks (TextBlock) -> files (FtlFile)
//
// A block with exactly one file is "shared" (common to all variants).
// A block with several files holds the variants: each file has a label
// (e.g. "UM Słupca") and the label is the key used to pick the right file
// when collective .ftl files are generated.
// ---------------------------------------------------------------------------

enum class ViewMode { Normal, Minimized, Maximized };

struct FtlFile {
    QString id;
    QString label;
    QString path;    // file on disk this text was opened from / saved to; empty if none
    QString content;
    ViewMode view = ViewMode::Normal;
};

struct TextBlock {
    QString id;
    QString label;
    QVector<FtlFile> files;

    bool isShared() const { return files.size() <= 1; }
};

struct Project {
    QString filePath;            // .ftlz file, empty if never saved
    QString collectivePrefix;    // "Zbiorcza nazwa"
    QVector<TextBlock> blocks;
    bool dirty = false;
};

QString newId();

TextBlock *findBlock(Project &p, const QString &blockId);
int blockIndex(const Project &p, const QString &blockId);
FtlFile *findFile(TextBlock &b, const QString &fileId);

// Unique, non-empty labels of all files in all blocks, sorted.
QStringList allFileLabels(const Project &p);

// ---------------------------------------------------------------------------
// .ftlz persistence (UTF-8 JSON)
// ---------------------------------------------------------------------------

QJsonObject projectToJson(const Project &p);
bool projectFromJson(const QJsonObject &root, Project &out, QString &error);
bool readProjectFile(const QString &path, Project &out, QString &error);
bool writeProjectFile(const Project &p, const QString &path, QString &error);

bool readTextFile(const QString &path, QString &out, QString &error);
bool writeTextFile(const QString &path, const QString &content, QString &error);

// ---------------------------------------------------------------------------
// Collective .ftl generation
// ---------------------------------------------------------------------------

struct GeneratedFile {
    QString name;     // e.g. "podatek_UM_Słupca.ftl"
    QString content;
};

struct GenerateResult {
    QVector<GeneratedFile> files;
    QStringList errors;
};

// A variant block that has no file for a label used by some other variant block.
struct MissingVariant {
    QString blockLabel;
    QString label;
};

// Variant blocks (blocks with several files) that lack a label used in another
// variant block. Blocks are listed in document order, labels alphabetically.
QVector<MissingVariant> findMissingVariants(const Project &p);

// Replaces whitespace with '_' and drops characters that are illegal in file names.
QString sanitizeFileName(const QString &name);

// One output file per distinct variant label. Each output concatenates, in
// block order: every shared block, and the variant of each multi-file block
// whose label matches.
GenerateResult generateCollective(const Project &p);
