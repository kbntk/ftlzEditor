#pragma once

#include <QString>

// Indentation unit used by autoIndentFtl: a tab, or a number of spaces (1..9).
struct IndentSettings {
    bool useTabs = false;
    int spaces = 4;

    QString unit() const;
};

IndentSettings loadIndentSettings();
void saveIndentSettings(const IndentSettings &settings);

// Re-indents the FreeMarker block structure of a template: the content of
// <#if>, <#list>, <#macro>, <@userdirective> ... and similar blocks is indented
// by one indentation unit (4 spaces by default) per nesting level. The content of HTML block elements (<html>,
// <head>, <body>, <div>, <table>, <style>, ...) is indented the same way.
// <#else>, <#elseif>, <#case>, <#default> and
// <#recover> are aligned with their opening tag. Leading and trailing whitespace
// of each line is normalised. Lines inside multi-line <#-- --> comments are kept
// unchanged. Text and HTML/CSS are otherwise left as they are.
QString autoIndentFtl(const QString &text, const QString &indentUnit = QStringLiteral("    "));
