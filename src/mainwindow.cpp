#include "mainwindow.h"

#include "blockwidget.h"
#include "i18n.h"
#include "indent.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QVBoxLayout>

#include <optional>
#include <utility>

namespace {

constexpr int kMaxRecentProjects = 25;

TextBlock makeBlock(const QString &label)
{
    TextBlock b;
    b.id = newId();
    b.label = label;
    FtlFile f;
    f.id = newId();
    f.label = label;
    b.files.append(f);
    return b;
}

FtlFile makeFile(const QString &label, const QString &path, const QString &content)
{
    FtlFile f;
    f.id = newId();
    f.label = label;
    f.path = path;
    f.content = content;
    return f;
}

QStringList recentProjects()
{
    QSettings settings;
    QStringList result;
    for (const QString &path : settings.value(QStringLiteral("recentProjects")).toStringList()) {
        if (QFileInfo::exists(path))
            result << path;
        if (result.size() == kMaxRecentProjects)
            break;
    }
    return result;
}

void storeRecentProjects(const QStringList &paths)
{
    QSettings settings;
    settings.setValue(QStringLiteral("recentProjects"), paths.mid(0, kMaxRecentProjects));
}

void addRecentProject(const QString &path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    QStringList paths = QSettings().value(QStringLiteral("recentProjects")).toStringList();
    paths.removeAll(absolute);
    paths.prepend(absolute);
    storeRecentProjects(paths);
}

void removeRecentProject(const QString &path)
{
    QStringList paths = QSettings().value(QStringLiteral("recentProjects")).toStringList();
    paths.removeAll(QFileInfo(path).absoluteFilePath());
    storeRecentProjects(paths);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_lastDir(QDir::homePath())
{
    resize(1400, 900);
    m_project.blocks.append(makeBlock(tx("lbl.block").arg(1)));
    buildMenus();
    buildCentral();
    updateTitle();
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void MainWindow::buildMenus()
{
    // Old menus are deleted later: this may be called from one of their actions.
    for (QMenu *menu : menuBar()->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly))
        menu->deleteLater();
    menuBar()->clear();

    QMenu *file = menuBar()->addMenu(tx("menu.file"));
    file->addAction(tx("act.newProject"), this, &MainWindow::newProject, QKeySequence::New);
    file->addAction(tx("act.openProject"), this, &MainWindow::openProject, QKeySequence::Open);
    m_recentMenu = file->addMenu(tx("menu.openRecent"));
    connect(m_recentMenu, &QMenu::aboutToShow, this, &MainWindow::populateRecentMenu);
    file->addAction(tx("act.openFtl"), this, &MainWindow::openFtlAsBlocks);
    file->addSeparator();
    file->addAction(tx("act.saveProject"), this, &MainWindow::saveProject, QKeySequence::Save);
    file->addAction(tx("act.saveProjectAs"), this, &MainWindow::saveProjectAs, QKeySequence::SaveAs);
    file->addSeparator();
    file->addAction(tx("act.quit"), this, &QWidget::close, QKeySequence::Quit);

    QMenu *language = menuBar()->addMenu(tx("menu.language"));
    auto *group = new QActionGroup(language);
    const QStringList names{QStringLiteral("&English"), QStringLiteral("&Deutsch")};
    for (int i = 0; i < names.size(); ++i) {
        QAction *action = language->addAction(names.at(i));
        action->setCheckable(true);
        action->setChecked((i == 0) == (currentLanguage() == Language::English));
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, i] { setLanguage(i); });
    }

    // "Options" and "About" are top-level entries of the main menu bar, not submenus.
    for (QAction *old : menuBar()->findChildren<QAction *>(QString(), Qt::FindDirectChildrenOnly))
        old->deleteLater();
    auto *options = new QAction(tx("menu.options"), menuBar());
    connect(options, &QAction::triggered, this, &MainWindow::showOptions);
    menuBar()->addAction(options);

    auto *about = new QAction(tx("menu.about"), menuBar());
    connect(about, &QAction::triggered, this, &MainWindow::showAbout);
    menuBar()->addAction(about);
}

void MainWindow::showOptions()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tx("opt.title"));

    const IndentSettings current = loadIndentSettings();
    auto *group = new QGroupBox(tx("opt.autoIndentWith"), &dialog);
    auto *tabs = new QRadioButton(tx("opt.tab"), group);
    auto *spaces = new QRadioButton(tx("opt.spaces"), group);
    auto *count = new QSpinBox(group);
    count->setRange(1, 9);
    count->setValue(current.spaces);
    (current.useTabs ? tabs : spaces)->setChecked(true);
    count->setEnabled(!current.useTabs);
    connect(spaces, &QRadioButton::toggled, count, &QSpinBox::setEnabled);

    auto *spacesRow = new QHBoxLayout;
    spacesRow->addWidget(spaces);
    spacesRow->addWidget(count);
    spacesRow->addStretch(1);
    auto *groupLayout = new QVBoxLayout(group);
    groupLayout->addWidget(tabs);
    groupLayout->addLayout(spacesRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tx("btn.ok"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tx("btn.cancel"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(group);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;
    IndentSettings chosen;
    chosen.useTabs = tabs->isChecked();
    chosen.spaces = count->value();
    saveIndentSettings(chosen);
    statusBar()->showMessage(tx("msg.optionsSaved"), 3000);
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tx("about.title"), tx("about.text"));
}

void MainWindow::buildCentral()
{
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    // --- top bar ---
    auto *top = new QHBoxLayout;
    auto addTop = [&](const QString &text, auto slot) {
        auto *btn = new QPushButton(text, central);
        connect(btn, &QPushButton::clicked, this, slot);
        top->addWidget(btn);
    };
    addTop(tx("btn.newBlock"), &MainWindow::addNewBlock);
    addTop(tx("btn.openFtl"), &MainWindow::openFtlAsBlocks);
    top->addSpacing(16);
    addTop(tx("btn.saveProject"), &MainWindow::saveProject);
    addTop(tx("btn.saveProjectAs"), &MainWindow::saveProjectAs);
    top->addSpacing(16);
    auto *autoIndent = new QPushButton(tx("btn.autoIndentDoc"), central);
    connect(autoIndent, &QPushButton::clicked, this, &MainWindow::autoIndentDocument);
    top->addWidget(autoIndent);
    top->addStretch(1);
    root->addLayout(top);

    // --- blocks ---
    auto *scroll = new QScrollArea(central);
    scroll->setWidgetResizable(true);
    m_blocksHost = new QWidget(scroll);
    m_blocksLayout = new QVBoxLayout(m_blocksHost);
    m_blocksLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(m_blocksHost);
    root->addWidget(scroll, 1);

    // --- bottom bar: collective name and generation ---
    auto *bottom = new QHBoxLayout;
    bottom->addWidget(new QLabel(tx("lbl.collectiveName"), central));
    m_prefixEdit = new QLineEdit(m_project.collectivePrefix, central);
    connect(m_prefixEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        m_project.collectivePrefix = text;
        markDirty();
    });
    bottom->addWidget(m_prefixEdit, 1);
    auto *generate = new QPushButton(tx("btn.generate"), central);
    connect(generate, &QPushButton::clicked, this, &MainWindow::generateFiles);
    bottom->addWidget(generate);
    root->addLayout(bottom);

    setCentralWidget(central);
    rebuildBlocks();
}

void MainWindow::refreshAll()
{
    m_prefixEdit->setText(m_project.collectivePrefix);
    rebuildBlocks();
    updateTitle();
}

void MainWindow::rebuildBlocks()
{
    while (QLayoutItem *item = m_blocksLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }

    for (int i = 0; i < m_project.blocks.size(); ++i) {
        const QString id = m_project.blocks.at(i).id;
        auto *widget = new BlockWidget(&m_project, id, m_blocksHost);
        widget->setShaded(i % 2 == 1); // 2nd, 4th, ... block is darker
        connect(widget, &BlockWidget::commandRequested, this,
                [this, id](const QString &fileId, Command cmd) { handleCommand(id, fileId, cmd); });
        m_blocksLayout->addWidget(widget);
    }
    m_blocksLayout->addStretch(1);
}

void MainWindow::setLanguage(int languageIndex)
{
    const Language lang = languageIndex == 0 ? Language::English : Language::German;
    if (lang == currentLanguage())
        return;
    setCurrentLanguage(lang);
    saveLanguage(lang);
    buildMenus();
    buildCentral();
    updateTitle();
}

// ---------------------------------------------------------------------------
// Dirty state and messages
// ---------------------------------------------------------------------------

void MainWindow::markDirty()
{
    m_project.dirty = true;
    updateTitle();
}

void MainWindow::updateTitle()
{
    const QString name = m_project.filePath.isEmpty() ? tx("doc.untitled")
                                                      : QFileInfo(m_project.filePath).fileName();
    setWindowTitle(QStringLiteral("%1%2 — %3")
                       .arg(name, m_project.dirty ? QStringLiteral(" *") : QString(), tx("app.title")));
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::warning(this, tx("msg.error"), message);
}

bool MainWindow::askYesNo(const QString &title, const QString &question)
{
    QMessageBox box(QMessageBox::Question, title, question, QMessageBox::NoButton, this);
    QPushButton *yes = box.addButton(tx("btn.yes"), QMessageBox::AcceptRole);
    box.addButton(tx("btn.no"), QMessageBox::RejectRole);
    box.exec();
    return box.clickedButton() == yes;
}

// ---------------------------------------------------------------------------
// Project (.ftlz) actions
// ---------------------------------------------------------------------------

bool MainWindow::confirmDiscardChanges()
{
    if (!m_project.dirty)
        return true;

    QMessageBox box(QMessageBox::Question, tx("app.title"), tx("msg.unsaved"), QMessageBox::NoButton, this);
    QPushButton *save = box.addButton(tx("btn.save"), QMessageBox::AcceptRole);
    QPushButton *discard = box.addButton(tx("btn.discard"), QMessageBox::DestructiveRole);
    box.addButton(tx("btn.cancel"), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == save)
        return saveProject();
    return box.clickedButton() == discard;
}

void MainWindow::newProject()
{
    if (!confirmDiscardChanges())
        return;
    m_project = Project();
    m_project.blocks.append(makeBlock(tx("lbl.block").arg(1)));
    refreshAll();
}

void MainWindow::openProject()
{
    if (!confirmDiscardChanges())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tx("dlg.openFtlz"), m_lastDir,
                                                      tx("filter.ftlz") + QStringLiteral(";;") + tx("filter.all"));
    if (path.isEmpty())
        return;
    m_lastDir = QFileInfo(path).absolutePath();
    loadProject(path);
}

bool MainWindow::loadProject(const QString &path)
{
    Project loaded;
    QString error;
    if (!readProjectFile(path, loaded, error)) {
        showError(tx("msg.projectLoadFailed").arg(path, error));
        removeRecentProject(path);
        return false;
    }
    m_project = loaded;
    refreshAll();
    addRecentProject(path);
    statusBar()->showMessage(tx("msg.saved"), 3000);
    return true;
}

void MainWindow::openRecentProject(const QString &path)
{
    if (!confirmDiscardChanges())
        return;
    loadProject(path);
}

void MainWindow::populateRecentMenu()
{
    m_recentMenu->clear();
    const QStringList files = recentProjects();
    if (files.isEmpty()) {
        m_recentMenu->addAction(tx("menu.noRecent"))->setEnabled(false);
        return;
    }
    for (const QString &path : files) {
        QAction *action = m_recentMenu->addAction(QDir::toNativeSeparators(path));
        connect(action, &QAction::triggered, this, [this, path] { openRecentProject(path); });
    }
}

bool MainWindow::saveProject()
{
    if (m_project.filePath.isEmpty())
        return saveProjectAs();

    QString error;
    if (!writeProjectFile(m_project, m_project.filePath, error)) {
        showError(tx("msg.projectSaveFailed").arg(m_project.filePath, error));
        return false;
    }
    m_project.dirty = false;
    addRecentProject(m_project.filePath);
    updateTitle();
    statusBar()->showMessage(tx("msg.saved"), 3000);
    return true;
}

bool MainWindow::saveProjectAs()
{
    QString path = QFileDialog::getSaveFileName(this, tx("dlg.saveFtlz"),
                                                QDir(m_lastDir).filePath(QStringLiteral("untitled.ftlz")),
                                                tx("filter.ftlz"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QLatin1String(".ftlz"), Qt::CaseInsensitive))
        path += QStringLiteral(".ftlz");
    m_lastDir = QFileInfo(path).absolutePath();

    const QString previous = m_project.filePath;
    m_project.filePath = path;
    if (!saveProject()) {
        m_project.filePath = previous;
        updateTitle();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Block actions
// ---------------------------------------------------------------------------

std::optional<int> MainWindow::askInsertPosition(const QString &title)
{
    // Each entry maps to the index the new block gets in m_project.blocks.
    QStringList items;
    QVector<int> indexes;

    items << tx("pos.beginning");
    indexes << 0;
    for (int i = 0; i < m_project.blocks.size(); ++i) {
        const TextBlock &b = m_project.blocks.at(i);
        const QString name = QStringLiteral("%1. %2").arg(i + 1).arg(b.label.isEmpty() ? tx("lbl.noLabel") : b.label);
        items << tx("pos.after").arg(name);
        indexes << i + 1;
    }
    items << tx("pos.end");
    indexes << m_project.blocks.size();

    bool ok = false;
    const QString choice = QInputDialog::getItem(this, title, tx("pos.question"), items,
                                                 items.size() - 1, false, &ok);
    if (!ok)
        return std::nullopt;
    return indexes.at(items.indexOf(choice));
}

void MainWindow::addNewBlock()
{
    const std::optional<int> at = askInsertPosition(tx("btn.newBlock"));
    if (!at)
        return;
    m_project.blocks.insert(*at, makeBlock(tx("lbl.block").arg(m_project.blocks.size() + 1)));
    markDirty();
    rebuildBlocks();
}

void MainWindow::openFtlAsBlocks()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, tx("dlg.openFtl"), m_lastDir,
                                                            tx("filter.ftl") + QStringLiteral(";;") + tx("filter.all"));
    if (paths.isEmpty())
        return;
    m_lastDir = QFileInfo(paths.first()).absolutePath();

    const std::optional<int> at = askInsertPosition(tx("btn.openFtl"));
    if (!at)
        return;
    int position = *at;

    for (const QString &path : paths) {
        QString content, error;
        if (!readTextFile(path, content, error)) {
            showError(tx("msg.readFailed").arg(path, error));
            continue;
        }
        const QString label = QFileInfo(path).completeBaseName();
        TextBlock b;
        b.id = newId();
        b.label = label;
        b.files.append(makeFile(label, path, content));
        m_project.blocks.insert(position++, b); // selected files keep their order
    }
    markDirty();
    rebuildBlocks();
}

void MainWindow::handleCommand(const QString &blockId, const QString &fileId, Command cmd)
{
    TextBlock *b = findBlock(m_project, blockId);
    switch (cmd) {
    case Command::Changed:
        markDirty();
        return;
    case Command::SaveBlock:
        saveBlock(blockId, false);
        return;
    case Command::SaveBlockAs:
        saveBlock(blockId, true);
        return;
    case Command::AddFromFile:
        addVariantsFromFiles(blockId);
        return;
    case Command::AddEmpty:
        addEmptyVariant(blockId);
        return;
    case Command::MoveUp:
        moveBlock(blockId, -1);
        return;
    case Command::MoveDown:
        moveBlock(blockId, 1);
        return;
    case Command::DeleteBlock:
        deleteBlock(blockId);
        return;
    case Command::AutoIndentBlock:
        autoIndentBlock(blockId);
        return;
    case Command::DeleteFile:
        deleteFile(blockId, fileId);
        return;
    case Command::AutoIndentFile:
        autoIndentFile(blockId, fileId);
        return;
    }
}

void MainWindow::saveBlock(const QString &blockId, bool saveAs)
{
    TextBlock *b = findBlock(m_project, blockId);
    if (!b || b->files.isEmpty())
        return;

    const QString defaultDir = m_lastDir;
    bool pathsChanged = false;

    if (saveAs && b->files.size() == 1) {
        FtlFile &f = b->files.first();
        const QString suggested = f.path.isEmpty()
                                      ? QDir(defaultDir).filePath(sanitizeFileName(f.label) + QStringLiteral(".ftl"))
                                      : f.path;
        const QString path = QFileDialog::getSaveFileName(this, tx("dlg.saveAs"), suggested, tx("filter.ftl"));
        if (path.isEmpty())
            return;
        f.path = path;
        pathsChanged = true;
    } else if (saveAs) {
        const QString dir = QFileDialog::getExistingDirectory(this, tx("dlg.chooseDir"), defaultDir);
        if (dir.isEmpty())
            return;
        for (FtlFile &f : b->files)
            f.path = QDir(dir).filePath(sanitizeFileName(f.label) + QStringLiteral(".ftl"));
        pathsChanged = true;

        QStringList existing;
        for (const FtlFile &f : std::as_const(b->files))
            if (QFileInfo::exists(f.path))
                existing << QFileInfo(f.path).fileName();
        if (!existing.isEmpty() &&
            !askYesNo(tx("dlg.saveAs"), tx("msg.overwrite").arg(existing.join(QLatin1String(", ")))))
            return;
    } else {
        // "Save": files that were never saved get a Save As dialog.
        for (FtlFile &f : b->files) {
            if (!f.path.isEmpty())
                continue;
            const QString path = QFileDialog::getSaveFileName(
                this, tx("dlg.saveAs"), QDir(defaultDir).filePath(sanitizeFileName(f.label) + QStringLiteral(".ftl")),
                tx("filter.ftl"));
            if (path.isEmpty())
                return;
            f.path = path;
            pathsChanged = true;
        }
    }

    if (!b->files.isEmpty())
        m_lastDir = QFileInfo(b->files.first().path).absolutePath();

    for (const FtlFile &f : std::as_const(b->files)) {
        QString error;
        if (!writeTextFile(f.path, f.content, error)) {
            showError(tx("msg.writeFailed").arg(f.path, error));
            return;
        }
    }

    if (pathsChanged) {
        markDirty();
        rebuildBlocks(); // refresh the file names shown in the panes
    }
    statusBar()->showMessage(tx("msg.saved"), 3000);
}

void MainWindow::addVariantsFromFiles(const QString &blockId)
{
    TextBlock *b = findBlock(m_project, blockId);
    if (!b)
        return;
    const QStringList paths = QFileDialog::getOpenFileNames(this, tx("dlg.openFtl"), m_lastDir,
                                                            tx("filter.ftl") + QStringLiteral(";;") + tx("filter.all"));
    if (paths.isEmpty())
        return;
    m_lastDir = QFileInfo(paths.first()).absolutePath();

    for (const QString &path : paths) {
        QString content, error;
        if (!readTextFile(path, content, error)) {
            showError(tx("msg.readFailed").arg(path, error));
            continue;
        }
        b->files.append(makeFile(QFileInfo(path).completeBaseName(), path, content));
    }
    markDirty();
    rebuildBlocks();
}

void MainWindow::addEmptyVariant(const QString &blockId)
{
    TextBlock *b = findBlock(m_project, blockId);
    if (!b)
        return;
    b->files.append(makeFile(tx("lbl.variant").arg(b->files.size() + 1), QString(), QString()));
    markDirty();
    rebuildBlocks();
}

void MainWindow::moveBlock(const QString &blockId, int delta)
{
    const int from = blockIndex(m_project, blockId);
    const int to = from + delta;
    if (from < 0 || to < 0 || to >= m_project.blocks.size())
        return;
    std::swap(m_project.blocks[from], m_project.blocks[to]);
    markDirty();
    rebuildBlocks();
}

void MainWindow::deleteBlock(const QString &blockId)
{
    const int index = blockIndex(m_project, blockId);
    if (index < 0)
        return;
    if (!askYesNo(tx("btn.delete"), tx("msg.deleteBlock")))
        return;
    m_project.blocks.remove(index);
    markDirty();
    rebuildBlocks();
}

void MainWindow::deleteFile(const QString &blockId, const QString &fileId)
{
    const int blockPos = blockIndex(m_project, blockId);
    if (blockPos < 0)
        return;
    TextBlock &b = m_project.blocks[blockPos];

    for (int i = 0; i < b.files.size(); ++i) {
        if (b.files[i].id != fileId)
            continue;
        const FtlFile &f = b.files[i];
        if (!f.content.isEmpty() && !askYesNo(tx("btn.delete"), tx("msg.deleteFile").arg(f.label)))
            return;
        b.files.remove(i);
        break;
    }
    if (b.files.isEmpty())
        m_project.blocks.remove(blockPos); // a block always has at least one file
    markDirty();
    rebuildBlocks();
}

namespace {

// Re-indents one file; returns true if its content changed.
bool indentFile(FtlFile &f)
{
    const QString indented = autoIndentFtl(f.content, loadIndentSettings().unit());
    if (indented == f.content)
        return false;
    f.content = indented;
    return true;
}

} // namespace

void MainWindow::autoIndentDocument()
{
    bool changed = false;
    for (TextBlock &b : m_project.blocks)
        for (FtlFile &f : b.files)
            changed = indentFile(f) || changed;
    if (changed) {
        markDirty();
        rebuildBlocks();
    }
    statusBar()->showMessage(tx("msg.indented"), 3000);
}

void MainWindow::autoIndentBlock(const QString &blockId)
{
    TextBlock *b = findBlock(m_project, blockId);
    if (!b)
        return;
    bool changed = false;
    for (FtlFile &f : b->files)
        changed = indentFile(f) || changed;
    if (changed) {
        markDirty();
        rebuildBlocks();
    }
    statusBar()->showMessage(tx("msg.indented"), 3000);
}

void MainWindow::autoIndentFile(const QString &blockId, const QString &fileId)
{
    TextBlock *b = findBlock(m_project, blockId);
    FtlFile *f = b ? findFile(*b, fileId) : nullptr;
    if (!f)
        return;
    if (indentFile(*f)) {
        markDirty();
        rebuildBlocks();
    }
    statusBar()->showMessage(tx("msg.indented"), 3000);
}

// ---------------------------------------------------------------------------
// Collective generation
// ---------------------------------------------------------------------------

void MainWindow::generateFiles()
{
    const GenerateResult result = generateCollective(m_project);
    if (!result.errors.isEmpty()) {
        showError(result.errors.join(QLatin1Char('\n')));
        return;
    }

    const QString dir = QFileDialog::getExistingDirectory(this, tx("dlg.chooseDir"), m_lastDir);
    if (dir.isEmpty())
        return;
    m_lastDir = dir;

    QStringList existing;
    for (const GeneratedFile &g : result.files)
        if (QFileInfo::exists(QDir(dir).filePath(g.name)))
            existing << g.name;
    if (!existing.isEmpty() &&
        !askYesNo(tx("btn.generate"), tx("msg.overwrite").arg(existing.join(QLatin1Char('\n')))))
        return;

    for (const GeneratedFile &g : result.files) {
        QString error;
        if (!writeTextFile(QDir(dir).filePath(g.name), g.content, error)) {
            showError(tx("msg.writeFailed").arg(QDir(dir).filePath(g.name), error));
            return;
        }
    }
    QMessageBox::information(this, tx("app.title"),
                             tx("msg.generated").arg(result.files.size()).arg(QDir::toNativeSeparators(dir)));
}

// ---------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscardChanges())
        event->accept();
    else
        event->ignore();
}
