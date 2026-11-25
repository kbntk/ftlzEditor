# FTLZ Editor

[English](#english) · [Deutsch](#deutsch)

---

## English

A text block editor for Apache FreeMarker (`.ftl`) templates, written in Qt 6 (Widgets, C++17).
Runs on Linux and Windows. The interface is available in English and German (menu *Language / Sprache*).

### What it does

**Text blocks and variants**
- A block is one or more files. A block with **one file** is shared by all variants (e.g. the body of a letter, a CSS stylesheet).
- A block with **several files** holds the variants (e.g. office name, stamp, opening hours). Each file has a **label** such as `UM Słupca`.
- The same file label can appear in many blocks, so one variant (`UM Słupca`) collects its files across blocks.
- Blocks can be created empty, opened from `.ftl` files, or extended with variants from files or as empty variants.
- Blocks can be reordered (up/down) and deleted. Variants can be deleted too.

**Labels**
- A file's initial label is its file name without the extension; labels are editable.
- While you type a label, matching existing labels are suggested. Matching is on the start of a word: `Słu` suggests `UM Słupca`, not `UG Strzałkowo`.
- Each block also has its own label.

**Editing and files**
- Edit text directly in each variant's editor, with FreeMarker syntax highlighting: directives (`<#if>`, `</#list>`, `<@macro>`, `[#…]`), interpolations (`${…}`, `#{…}`), string literals and multi-line comments (`<#-- … -->`).
- Each block remembers the `.ftl` file each variant was opened from. **Save** writes every variant back to its file; files that were never saved ask for a name. **Save as…** writes to a new location (for a block with several variants, you choose a folder and each variant is saved as `<label>.ftl`).

**Views and layout**
- Variants of a block are shown side by side, sorted alphabetically by label.
- Every editor is 120 characters wide.
- Each variant can be **minimized** (1 line), shown **normal** (20 lines), or **maximized** (all lines), so long files (300+ lines) don't take over the screen.
- A **filter** in each block's header shows only variants whose label contains the typed text, case-insensitive (`ug` shows both `UG Strzałkowo` and `UG Powidz`).

**Auto indent**
- **Auto indent document** (top bar) re-indents every variant of every block.
- **Auto indent** in a block header re-indents all variants of that block; the button on a variant re-indents that variant only.
- Indentation follows the FreeMarker structure: the content of `<#if>`, `<#list>`, `<#macro>`, `<#switch>`, `<@userdirective>` and similar blocks is indented by 4 spaces per level. `<#else>`, `<#elseif>`, `<#case>`, `<#default>` and `<#recover>` align with their opening tag.
- Lines inside multi-line `<#-- … -->` comments are left unchanged. Plain text and HTML/CSS are only moved, not rewritten.

**Project file (`.ftlz`)**
- Saves everything needed to resume work: blocks, variant contents, the `.ftl` paths they were opened from, labels, view modes and the collective name.
- New, Open, Save and Save as for `.ftlz`. Unsaved changes trigger a prompt before closing or opening another project.
- Format: UTF-8 JSON with `"format": "ftlz"` and `"version": 1`.

**Collective `.ftl` generation**
- Enter a **collective name** (e.g. `podatek_`) and click **Generate collective .ftl files**.
- One file is produced per variant label. Each file contains, in block order, every shared block and the matching variant from each multi-file block.
- Example: collective name `podatek_`, labels `UM_Słupca` and `UG_Strzałkowo` → `podatek_UM_Słupca.ftl` and `podatek_UG_Strzałkowo.ftl`.
- Spaces in labels become underscores in file names. The tool reports problems before writing: missing labels, duplicate labels within one block, and two labels producing the same file name. Existing files are overwritten only after confirmation.

### Build

Requirements: Qt 6 (Core, Gui, Widgets; Test optional), CMake ≥ 3.21, a C++17 compiler.

```sh
cmake -S . -B build
cmake --build build
./build/ftlz-edytor          # the application
./build/ftlcore_tests        # tests for format, generation and label matching
```

Windows (Qt 6 for MSVC or MinGW):

```bat
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.x/msvc2022_64
cmake --build build
windeployqt build\ftlz-edytor.exe
```

---

## Deutsch

Ein Editor für Textblöcke mit Apache-FreeMarker-Vorlagen (`.ftl`), geschrieben in Qt 6 (Widgets, C++17).
Läuft unter Linux und Windows. Die Oberfläche gibt es auf Deutsch und Englisch (Menü *Sprache / Language*).

### Funktionsumfang

**Textblöcke und Varianten**
- Ein Block besteht aus einer oder mehreren Dateien. Ein Block mit **einer Datei** ist für alle Varianten gemeinsam (z. B. Haupttext eines Schreibens, CSS).
- Ein Block mit **mehreren Dateien** enthält die Varianten (z. B. Behördenname, Stempel, Öffnungszeiten). Jede Datei hat eine **Bezeichnung**, etwa `UM Słupca`.
- Dieselbe Bezeichnung darf in mehreren Blöcken vorkommen, sodass eine Variante (`UM Słupca`) ihre Dateien über alle Blöcke hinweg bündelt.
- Blöcke können leer angelegt, aus `.ftl`-Dateien geöffnet oder um Varianten aus Dateien bzw. leere Varianten erweitert werden.
- Blöcke lassen sich verschieben (nach oben/unten) und löschen; ebenso einzelne Varianten.

**Bezeichnungen**
- Die Anfangsbezeichnung einer Datei ist ihr Dateiname ohne Erweiterung; Bezeichnungen sind editierbar.
- Während der Eingabe werden passende vorhandene Bezeichnungen vorgeschlagen. Abgeglichen wird am Wortanfang: `Słu` schlägt `UM Słupca` vor, nicht `UG Strzałkowo`.
- Auch jeder Block hat eine eigene Bezeichnung.

**Bearbeiten und Dateien**
- Der Text jeder Variante wird direkt im Editor bearbeitet, mit Syntaxhervorhebung für FreeMarker: Direktiven (`<#if>`, `</#list>`, `<@makro>`, `[#…]`), Interpolationen (`${…}`, `#{…}`), Zeichenketten und mehrzeilige Kommentare (`<#-- … -->`).
- Jeder Block merkt sich, aus welcher `.ftl`-Datei eine Variante geöffnet wurde. **Speichern** schreibt jede Variante in ihre Datei zurück; nicht gespeicherte Dateien fragen nach einem Namen. **Speichern unter…** schreibt an einen neuen Ort (bei einem Block mit mehreren Varianten wählt man einen Ordner, und jede Variante wird als `<Bezeichnung>.ftl` gespeichert).

**Ansichten und Layout**
- Die Varianten eines Blocks stehen nebeneinander, alphabetisch nach Bezeichnung sortiert.
- Jeder Editor ist 120 Zeichen breit.
- Jede Variante kann **minimiert** (1 Zeile), **normal** (20 Zeilen) oder **maximiert** (alle Zeilen) angezeigt werden, damit lange Dateien (300+ Zeilen) den Bildschirm nicht belegen.
- Ein **Filter** im Kopf jedes Blocks zeigt nur Varianten, deren Bezeichnung den eingegebenen Text enthält, ohne Berücksichtigung von Groß-/Kleinschreibung (`ug` zeigt `UG Strzałkowo` und `UG Powidz`).

**Automatisch einrücken**
- **Dokument automatisch einrücken** (obere Leiste) rückt jede Variante jedes Blocks ein.
- **Automatisch einrücken** im Blockkopf rückt alle Varianten dieses Blocks ein; der Knopf an einer Variante rückt nur diese Variante ein.
- Die Einrückung folgt der FreeMarker-Struktur: Der Inhalt von `<#if>`, `<#list>`, `<#macro>`, `<#switch>`, `<@Benutzerdirektive>` und ähnlichen Blöcken wird um 4 Leerzeichen pro Ebene eingerückt. `<#else>`, `<#elseif>`, `<#case>`, `<#default>` und `<#recover>` stehen auf der Höhe ihres öffnenden Tags.
- Zeilen innerhalb mehrzeiliger `<#-- … -->`-Kommentare bleiben unverändert. Normaler Text sowie HTML/CSS werden nur verschoben, nicht umgeschrieben.

**Projektdatei (`.ftlz`)**
- Speichert alles, um die Arbeit fortzusetzen: Blöcke, Inhalte der Varianten, die Pfade der geöffneten `.ftl`-Dateien, Bezeichnungen, Ansichtsmodi und den Sammelnamen.
- Neu, Öffnen, Speichern und Speichern unter für `.ftlz`. Ungespeicherte Änderungen führen beim Schließen oder Öffnen eines anderen Projekts zu einer Rückfrage.
- Format: UTF-8-JSON mit `"format": "ftlz"` und `"version": 1`.

**Sammel-`.ftl`-Dateien erzeugen**
- Einen **Sammelnamen** eingeben (z. B. `podatek_`) und auf **Sammel-.ftl-Dateien erzeugen** klicken.
- Pro Variantenbezeichnung entsteht eine Datei. Sie enthält in Blockreihenfolge alle gemeinsamen Blöcke und die passende Variante aus jedem Block mit mehreren Dateien.
- Beispiel: Sammelname `podatek_`, Bezeichnungen `UM_Słupca` und `UG_Strzałkowo` → `podatek_UM_Słupca.ftl` und `podatek_UG_Strzałkowo.ftl`.
- Leerzeichen in Bezeichnungen werden in Dateinamen durch Unterstriche ersetzt. Probleme werden vor dem Schreiben gemeldet: fehlende Bezeichnungen, doppelte Bezeichnungen in einem Block und zwei Bezeichnungen, die denselben Dateinamen ergeben. Vorhandene Dateien werden erst nach Bestätigung überschrieben.

### Erstellen

Voraussetzungen: Qt 6 (Core, Gui, Widgets; Test optional), CMake ≥ 3.21, ein C++17-Compiler.

```sh
cmake -S . -B build
cmake --build build
./build/ftlz-edytor          # die Anwendung
./build/ftlcore_tests        # Tests für Format, Erzeugung und Bezeichnungsabgleich
```

Windows (Qt 6 für MSVC oder MinGW):

```bat
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.x/msvc2022_64
cmake --build build
windeployqt build\ftlz-edytor.exe
```
