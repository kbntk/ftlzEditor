#include "i18n.h"

#include <QHash>
#include <QLocale>
#include <QSettings>

namespace {

Language g_language = Language::English;

struct Entry {
    const char *key;
    const char *en;
    const char *de;
};

const Entry kEntries[] = {
    // application / menus
    {"app.title", "FTLZ Editor", "FTLZ-Editor"},
    {"doc.untitled", "untitled", "unbenannt"},
    {"menu.file", "&File", "&Datei"},
    {"menu.language", "&Language", "&Sprache"},
    {"menu.about", "&About", "Ü&ber"},
    {"menu.options", "&Options", "&Optionen"},
    {"menu.edit", "&Edit", "Be&arbeiten"},
    {"act.undo", "&Undo", "&Rückgängig"},
    {"act.redo", "&Redo", "&Wiederholen"},
    {"msg.undone", "Undone", "Rückgängig gemacht"},
    {"msg.redone", "Redone", "Wiederholt"},
    {"opt.title", "Options", "Optionen"},
    {"opt.autoIndentWith", "Auto indent with", "Automatisch einrücken mit"},
    {"opt.tab", "Tabulator", "Tabulator"},
    {"opt.spaces", "Spaces", "Leerzeichen"},
    {"msg.optionsSaved", "Options saved", "Optionen gespeichert"},
    {"btn.ok", "OK", "OK"},
    {"about.title", "About", "Über"},
    {"menu.openRecent", "Open &recent", "&Zuletzt geöffnet"},
    {"menu.noRecent", "(none)", "(keine)"},
    {"about.text", "Designed by Jakub Zakrzewski (jakub@jakubzakrzewski.eu).\nProgrammed with Claude Code.",
     "Entworfen von Jakub Zakrzewski (jakub@jakubzakrzewski.eu).\nProgrammiert mit Claude Code."},
    {"act.newProject", "&New ftlz", "&Neue ftlz-Datei"},
    {"act.openProject", "&Open ftlz…", "&ftlz öffnen…"},
    {"act.openFtl", "Open .ftl as &block…", ".ftl als &Block öffnen…"},
    {"act.saveProject", "&Save ftlz", "ftlz &speichern"},
    {"act.saveProjectAs", "Save ftlz &as…", "ftlz speichern &unter…"},
    {"act.quit", "&Quit", "Beende&n"},

    // buttons
    {"btn.newBlock", "New empty block", "Neuer leerer Block"},
    {"pos.title", "Insert position", "Einfügeposition"},
    {"pos.question", "Insert the new block:", "Neuen Block einfügen:"},
    {"pos.beginning", "At the beginning", "Am Anfang"},
    {"pos.after", "After %1", "Nach %1"},
    {"pos.end", "At the end", "Am Ende"},
    {"ctx.moveToNewBlock", "Move to new block", "In neuen Block verschieben"},
    {"pos.moveQuestion", "Where should the new block go?", "Wo soll der neue Block stehen?"},
    {"pos.above", "Above the current block", "Über dem aktuellen Block"},
    {"pos.below", "Below the current block", "Unter dem aktuellen Block"},
    {"lbl.noLabel", "(no label)", "(ohne Bezeichnung)"},
    {"btn.openFtl", "Open .ftl as block…", ".ftl als Block öffnen…"},
    {"btn.saveProject", "Save ftlz", "ftlz speichern"},
    {"btn.saveProjectAs", "Save ftlz as…", "ftlz speichern unter…"},
    {"btn.generate", "Generate collective .ftl files", "Sammel-.ftl-Dateien erzeugen"},
    {"btn.save", "Save", "Speichern"},
    {"btn.saveAs", "Save as…", "Speichern unter…"},
    {"btn.addFromFile", "Add variant from file…", "Variante aus Datei…"},
    {"btn.addEmpty", "Add empty variant", "Leere Variante"},
    {"btn.up", "Move block up", "Block nach oben"},
    {"btn.down", "Move block down", "Block nach unten"},
    {"btn.delete", "Delete", "Löschen"},
    {"btn.autoIndentDoc", "Auto indent document", "Dokument automatisch einrücken"},
    {"btn.autoIndent", "Auto indent", "Automatisch einrücken"},
    {"btn.minimize", "Minimize", "Minimieren"},
    {"btn.maximize", "Maximize", "Maximieren"},
    {"btn.yes", "Yes", "Ja"},
    {"btn.no", "No", "Nein"},
    {"btn.cancel", "Cancel", "Abbrechen"},
    {"btn.discard", "Discard", "Verwerfen"},

    // labels
    {"lbl.collectiveName", "Collective name:", "Sammelname:"},
    {"lbl.filter", "Filter by label:", "Nach Bezeichnung filtern:"},
    {"lbl.fileLabel", "label", "Bezeichnung"},
    {"lbl.block", "Block %1", "Block %1"},
    {"lbl.variant", "Variant %1", "Variante %1"},
    {"lbl.notSaved", "(not saved)", "(nicht gespeichert)"},
    {"tag.shared", "shared", "gemeinsam"},
    {"tag.variants", "variants", "Varianten"},

    // dialogs
    {"dlg.openFtl", "Open .ftl file(s)", ".ftl-Datei(en) öffnen"},
    {"dlg.openFtlz", "Open ftlz", "ftlz öffnen"},
    {"dlg.saveFtlz", "Save ftlz", "ftlz speichern"},
    {"dlg.saveAs", "Save as", "Speichern unter"},
    {"dlg.chooseDir", "Choose folder", "Ordner wählen"},

    // file filters
    {"filter.ftl", "FTL files (*.ftl)", "FTL-Dateien (*.ftl)"},
    {"filter.ftlz", "FTLZ files (*.ftlz)", "FTLZ-Dateien (*.ftlz)"},
    {"filter.all", "All files (*)", "Alle Dateien (*)"},

    // messages
    {"msg.error", "Error", "Fehler"},
    {"msg.saved", "Saved", "Gespeichert"},
    {"msg.indented", "Indentation updated", "Einrückung aktualisiert"},
    {"msg.generated", "%1 file(s) written to:\n%2", "%1 Datei(en) geschrieben nach:\n%2"},
    {"msg.overwrite", "These files already exist and will be overwritten:\n%1\n\nContinue?",
     "Diese Dateien existieren bereits und werden überschrieben:\n%1\n\nFortfahren?"},
    {"msg.unsaved", "The project has unsaved changes. Save them?",
     "Das Projekt hat ungespeicherte Änderungen. Speichern?"},
    {"msg.deleteBlock", "Delete this block?", "Diesen Block löschen?"},
    {"msg.deleteFile", "Delete variant \"%1\"?", "Variante \"%1\" löschen?"},
    {"msg.readFailed", "Cannot read file:\n%1\n\n%2", "Datei kann nicht gelesen werden:\n%1\n\n%2"},
    {"msg.writeFailed", "Cannot write file:\n%1\n\n%2", "Datei kann nicht geschrieben werden:\n%1\n\n%2"},
    {"msg.projectLoadFailed", "Cannot open ftlz file:\n%1\n\n%2",
     "ftlz-Datei kann nicht geöffnet werden:\n%1\n\n%2"},
    {"msg.projectSaveFailed", "Cannot save ftlz file:\n%1\n\n%2",
     "ftlz-Datei kann nicht gespeichert werden:\n%1\n\n%2"},

    // errors
    {"err.notFtlz", "Not an ftlz file.", "Keine ftlz-Datei."},
    {"err.newerVersion", "This ftlz file was created by a newer version of the editor.",
     "Diese ftlz-Datei stammt von einer neueren Programmversion."},
    {"err.parse", "Invalid JSON: %1", "Ungültiges JSON: %1"},
    {"err.noLabel", "Block \"%1\" has a variant without a label.",
     "Block \"%1\" hat eine Variante ohne Bezeichnung."},
    {"err.duplicateLabel", "Label \"%1\" is used twice in block \"%2\".",
     "Bezeichnung \"%1\" kommt im Block \"%2\" zweimal vor."},
    {"err.noVariants", "No variants found. Generating files needs at least one block with more than one file.",
     "Keine Varianten gefunden. Für die Erzeugung wird mindestens ein Block mit mehr als einer Datei benötigt."},
    {"err.nameClash", "Two variants would produce the same file name: %1",
     "Zwei Varianten ergeben denselben Dateinamen: %1"},
};

const QHash<QByteArray, const Entry *> &index()
{
    static const QHash<QByteArray, const Entry *> idx = [] {
        QHash<QByteArray, const Entry *> h;
        for (const Entry &e : kEntries)
            h.insert(QByteArray(e.key), &e);
        return h;
    }();
    return idx;
}

} // namespace

Language currentLanguage()
{
    return g_language;
}

void setCurrentLanguage(Language lang)
{
    g_language = lang;
}

Language savedLanguage()
{
    QSettings settings;
    const QString code = settings.value(QStringLiteral("language")).toString();
    if (code == QLatin1String("de"))
        return Language::German;
    if (code == QLatin1String("en"))
        return Language::English;
    return QLocale::system().language() == QLocale::German ? Language::German : Language::English;
}

void saveLanguage(Language lang)
{
    QSettings settings;
    settings.setValue(QStringLiteral("language"), lang == Language::German ? QStringLiteral("de")
                                                                           : QStringLiteral("en"));
}

QString tx(const char *key)
{
    const auto &idx = index();
    const auto it = idx.constFind(QByteArray(key));
    if (it == idx.constEnd())
        return QString::fromUtf8(key);
    return QString::fromUtf8(g_language == Language::German ? (*it)->de : (*it)->en);
}
