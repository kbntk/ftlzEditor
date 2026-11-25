#include "document.h"
#include "indent.h"
#include "labels.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

namespace {

FtlFile file(const QString &label, const QString &content)
{
    FtlFile f;
    f.id = newId();
    f.label = label;
    f.content = content;
    return f;
}

TextBlock block(const QString &label, QVector<FtlFile> files)
{
    TextBlock b;
    b.id = newId();
    b.label = label;
    b.files = files;
    return b;
}

// Document from the plan: shared body + css, variant name/stamp blocks.
Project planExample()
{
    Project p;
    p.collectivePrefix = QStringLiteral("podatek_");
    p.blocks.append(block(QStringLiteral("css"), {file(QStringLiteral("css"), QStringLiteral("<style>x</style>\n"))}));
    p.blocks.append(block(QStringLiteral("nazwa i pieczątka"),
                          {file(QStringLiteral("UM Słupca"), QStringLiteral("Urząd Miasta Słupca\n")),
                           file(QStringLiteral("UG Strzałkowo"), QStringLiteral("Urząd Gminy Strzałkowo\n"))}));
    p.blocks.append(block(QStringLiteral("treść"), {file(QStringLiteral("treść"), QStringLiteral("Treść pisma\n"))}));
    p.blocks.append(block(QStringLiteral("godziny"),
                          {file(QStringLiteral("UG Powidz"), QStringLiteral("Pon-Pt 8-16\n")),
                           file(QStringLiteral("UM Słupca"), QStringLiteral("Pon-Pt 7-15\n"))}));
    return p;
}

} // namespace

class CoreTests : public QObject {
    Q_OBJECT

private slots:
    void suggestsByWordPrefix()
    {
        const QStringList labels{QStringLiteral("UM Słupca"), QStringLiteral("UG Strzałkowo")};
        QCOMPARE(suggestLabels(QStringLiteral("Słu"), labels), QStringList{QStringLiteral("UM Słupca")});
        QCOMPARE(suggestLabels(QStringLiteral("um"), labels), QStringList{QStringLiteral("UM Słupca")});
        QVERIFY(suggestLabels(QStringLiteral(""), labels).isEmpty());
    }

    void suggestionsPreferPrefixMatches()
    {
        const QStringList labels{QStringLiteral("Gmina UG"), QStringLiteral("UG Powidz"), QStringLiteral("UG Strzałkowo")};
        const QStringList result = suggestLabels(QStringLiteral("ug"), labels);
        QCOMPARE(result, (QStringList{QStringLiteral("UG Powidz"), QStringLiteral("UG Strzałkowo"),
                                      QStringLiteral("Gmina UG")}));
    }

    void filterIsCaseInsensitiveSubstring()
    {
        QVERIFY(labelMatchesFilter(QStringLiteral("UG Strzałkowo"), QStringLiteral("ug")));
        QVERIFY(labelMatchesFilter(QStringLiteral("UG Powidz"), QStringLiteral("ug")));
        QVERIFY(!labelMatchesFilter(QStringLiteral("UM Słupca"), QStringLiteral("ug")));
        QVERIFY(labelMatchesFilter(QStringLiteral("UM Słupca"), QString()));
    }

    void generatesOneFilePerVariantLabel()
    {
        const GenerateResult r = generateCollective(planExample());
        QVERIFY2(r.errors.isEmpty(), qPrintable(r.errors.join(QLatin1Char(';'))));
        QCOMPARE(r.files.size(), 3);

        // Output order follows first appearance of labels in the blocks.
        QCOMPARE(r.files[0].name, QStringLiteral("podatek_UM_Słupca.ftl"));
        QCOMPARE(r.files[0].content, QStringLiteral("<style>x</style>\n"
                                                    "Urząd Miasta Słupca\n"
                                                    "Treść pisma\n"
                                                    "Pon-Pt 7-15\n"));
        QCOMPARE(r.files[1].name, QStringLiteral("podatek_UG_Strzałkowo.ftl"));
        QCOMPARE(r.files[1].content, QStringLiteral("<style>x</style>\n"
                                                    "Urząd Gminy Strzałkowo\n"
                                                    "Treść pisma\n"));
        QCOMPARE(r.files[2].name, QStringLiteral("podatek_UG_Powidz.ftl"));
        QCOMPARE(r.files[2].content, QStringLiteral("<style>x</style>\n"
                                                    "Treść pisma\n"
                                                    "Pon-Pt 8-16\n"));
    }

    void reportsMissingVariantsPerBlock()
    {
        // The example from the request: A has 1,2 and B has 1,3.
        Project p;
        p.blocks.append(block(QStringLiteral("A"), {file(QStringLiteral("1"), QStringLiteral("a1")),
                                                    file(QStringLiteral("2"), QStringLiteral("a2"))}));
        p.blocks.append(block(QStringLiteral("B"), {file(QStringLiteral("1"), QStringLiteral("b1")),
                                                    file(QStringLiteral("3"), QStringLiteral("b3"))}));
        p.blocks.append(block(QStringLiteral("shared"), {file(QStringLiteral("x"), QStringLiteral("x"))}));
        const QVector<MissingVariant> m = findMissingVariants(p);
        QCOMPARE(m.size(), 2);
        QCOMPARE(m[0].blockLabel, QStringLiteral("A"));
        QCOMPARE(m[0].label, QStringLiteral("3"));
        QCOMPARE(m[1].blockLabel, QStringLiteral("B"));
        QCOMPARE(m[1].label, QStringLiteral("2"));

        Project complete;
        complete.blocks.append(block(QStringLiteral("A"), {file(QStringLiteral("1"), QString()), file(QStringLiteral("2"), QString())}));
        complete.blocks.append(block(QStringLiteral("B"), {file(QStringLiteral("2"), QString()), file(QStringLiteral("1"), QString())}));
        QVERIFY(findMissingVariants(complete).isEmpty());
    }

    void spacesInLabelsBecomeUnderscores()
    {
        QCOMPARE(sanitizeFileName(QStringLiteral("UM Słupca")), QStringLiteral("UM_Słupca"));
        QCOMPARE(sanitizeFileName(QStringLiteral("a/b:c")), QStringLiteral("abc"));
    }

    void reportsMissingLabelAndDuplicates()
    {
        Project p;
        p.blocks.append(block(QStringLiteral("x"), {file(QString(), QStringLiteral("1")), file(QStringLiteral("A"), QStringLiteral("2"))}));
        QVERIFY(!generateCollective(p).errors.isEmpty());

        Project d;
        d.blocks.append(block(QStringLiteral("x"), {file(QStringLiteral("A"), QStringLiteral("1")), file(QStringLiteral("A"), QStringLiteral("2"))}));
        QVERIFY(!generateCollective(d).errors.isEmpty());

        Project none;
        none.blocks.append(block(QStringLiteral("x"), {file(QStringLiteral("x"), QStringLiteral("1"))}));
        QVERIFY(!generateCollective(none).errors.isEmpty());
    }

    void ftlzRoundTripKeepsEverythingImportant()
    {
        Project p = planExample();
        p.blocks[1].files[0].view = ViewMode::Maximized;
        p.blocks[1].files[1].view = ViewMode::Minimized;
        p.blocks[1].files[1].path = QStringLiteral("/tmp/UG.ftl");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = QDir(dir.path()).filePath(QStringLiteral("doc.ftlz"));

        QString error;
        QVERIFY2(writeProjectFile(p, path, error), qPrintable(error));

        Project loaded;
        QVERIFY2(readProjectFile(path, loaded, error), qPrintable(error));

        QCOMPARE(loaded.collectivePrefix, p.collectivePrefix);
        QCOMPARE(loaded.blocks.size(), p.blocks.size());
        for (int i = 0; i < p.blocks.size(); ++i) {
            QCOMPARE(loaded.blocks[i].id, p.blocks[i].id);
            QCOMPARE(loaded.blocks[i].label, p.blocks[i].label);
            QCOMPARE(loaded.blocks[i].files.size(), p.blocks[i].files.size());
            for (int j = 0; j < p.blocks[i].files.size(); ++j) {
                const FtlFile &a = p.blocks[i].files[j];
                const FtlFile &b = loaded.blocks[i].files[j];
                QCOMPARE(b.id, a.id);
                QCOMPARE(b.label, a.label);
                QCOMPARE(b.path, a.path);
                QCOMPARE(b.content, a.content);
                QCOMPARE(b.view, a.view);
            }
        }
    }

    void indentsBlocksAndDirectives()
    {
        const QString in = QStringLiteral(
            "<#if x>\n"
            "<p>a</p>\n"
            "<#list items as i>\n"
            "${i}\n"
            "</#list>\n"
            "<#else>\n"
            "b\n"
            "</#if>\n");
        const QString expected = QStringLiteral(
            "<#if x>\n"
            "    <p>a</p>\n"
            "    <#list items as i>\n"
            "        ${i}\n"
            "    </#list>\n"
            "<#else>\n"
            "    b\n"
            "</#if>\n");
        QCOMPARE(autoIndentFtl(in), expected);
    }

    void indentIsIdempotent()
    {
        const QString once = autoIndentFtl(QStringLiteral(
            "<@m a=1>\n  x\n<#switch v>\n<#case 1>\nA\n<#break>\n<#default>\nB\n</#switch>\n</@m>"));
        QCOMPARE(autoIndentFtl(once), once);
    }

    void keepsMultiLineCommentBodies()
    {
        const QString in = QStringLiteral("<#--\n  keep   me\n-->\n<#if a>\nx\n</#if>");
        const QString expected = QStringLiteral("<#--\n  keep   me\n-->\n<#if a>\n    x\n</#if>");
        QCOMPARE(autoIndentFtl(in), expected);
    }

    void indentsHtmlElements()
    {
        const QString in = QStringLiteral(
            "<!DOCTYPE html>\n"
            "<html lang=\"pl\">\n"
            "<head>\n"
            "  <meta charset=\"UTF-8\">\n"
            "  <title>Zawiadomienie ${taxYear}</title>\n"
            "</head>\n"
            "<body>\n"
            "<#if taxYear < 2025>\n"
            "<p>old</p>\n"
            "<#else>\n"
            "<p>new</p>\n"
            "</#if>\n"
            "</body>\n"
            "</html>");
        const QString expected = QStringLiteral(
            "<!DOCTYPE html>\n"
            "<html lang=\"pl\">\n"
            "    <head>\n"
            "        <meta charset=\"UTF-8\">\n"
            "        <title>Zawiadomienie ${taxYear}</title>\n"
            "    </head>\n"
            "    <body>\n"
            "        <#if taxYear < 2025>\n"
            "            <p>old</p>\n"
            "        <#else>\n"
            "            <p>new</p>\n"
            "        </#if>\n"
            "    </body>\n"
            "</html>");
        QCOMPARE(autoIndentFtl(in), expected);
    }

    void indentsWithTabsOrCustomSpaces()
    {
        const QString in = QStringLiteral("<#if a>\nx\n<#if b>\ny\n</#if>\n</#if>");
        QCOMPARE(autoIndentFtl(in, QStringLiteral("\t")),
                 QStringLiteral("<#if a>\n\tx\n\t<#if b>\n\t\ty\n\t</#if>\n</#if>"));
        QCOMPARE(autoIndentFtl(in, QString(2, QLatin1Char(' '))),
                 QStringLiteral("<#if a>\n  x\n  <#if b>\n    y\n  </#if>\n</#if>"));
        IndentSettings s;
        s.spaces = 9;
        QCOMPARE(s.unit(), QString(9, QLatin1Char(' ')));
        s.useTabs = true;
        QCOMPARE(s.unit(), QStringLiteral("\t"));
    }

    void unbalancedClosersDoNotGoNegative()
    {
        QCOMPARE(autoIndentFtl(QStringLiteral("</#if>\nx")), QStringLiteral("</#if>\nx"));
    }

    void rejectsForeignJson()
    {
        Project out;
        QString error;
        QVERIFY(!projectFromJson(QJsonObject{{QStringLiteral("format"), QStringLiteral("zip")}}, out, error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_APPLESS_MAIN(CoreTests)
#include "tests.moc"
