#pragma once

#include <QString>

enum class Language { English, German };

Language currentLanguage();
void setCurrentLanguage(Language lang);

// Language remembered from the previous run; defaults to German for German
// locales and English otherwise.
Language savedLanguage();
void saveLanguage(Language lang);

// Looks up a UI string by key in the current language. Unknown keys are returned as-is.
QString tx(const char *key);
