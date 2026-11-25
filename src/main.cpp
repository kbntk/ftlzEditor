#include "i18n.h"
#include "mainwindow.h"

#include <QApplication>
#include <QProxyStyle>
#include <QStyle>

// Qt shows the underline of a menu mnemonic (the letter after '&') only while Alt
// is held. This style keeps the underlines visible, so the shortcuts can be read
// at a glance.
class AlwaysUnderlineStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                  QStyleHintReturn *returnData) const override
    {
        if (hint == SH_UnderlineShortcut)
            return 1;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ftlz-edytor"));
    QApplication::setOrganizationName(QStringLiteral("ftlz-edytor"));
    app.setStyle(new AlwaysUnderlineStyle(app.style()->name()));

    setCurrentLanguage(savedLanguage());

    MainWindow window;
    window.show();
    return app.exec();
}
