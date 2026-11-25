#include "mainwindow.h"

#include "blockwidget.h"
#include "i18n.h"
#include "indent.h"

#include <QAction>
#include <QApplication>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QPlainTextEdit>
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
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QVBoxLayout>

#include <optional>
#include <utility>

namespace {

constexpr int kMaxRecentProjects = 25;

constexpr int kMaxUndoSteps = 200;

bool isRedoKey(const QKeyEvent *ke)
{
    return ke->matches(QKeySequence::Redo) ||
           (ke->key() == Qt::Key_Y && ke->modifiers() == Qt::ControlModifier) ||
           (ke->key() == Qt::Key_Z && ke->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier));
}

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
    qApp->installEventFilter(this);
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

    QMenu *edit = menuBar()->addMenu(tx("menu.edit"));
    edit->addAction(tx("act.undo"), this, &MainWindow::undo, QKeySequence::Undo);
    QAction *redo = edit->addAction(tx("act.redo"), this, &MainWindow::redo);
    redo->setShortcuts({QKeySequence(QStringLiteral("Ctrl+Y")), QKeySequence(QStringLiteral("Ctrl+Shift+Z"))});

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
    QMessageBox box(QMessageBox::Information, tx("about.title"), tx("about.text"), QMessageBox::Ok, this);
    QPushButton *license = box.addButton(tx("about.license"), QMessageBox::ActionRole);
    box.exec();
    if (box.clickedButton() == license)
        showLicense();
}

void MainWindow::showLicense()
{
    QFile file(QStringLiteral(":/LICENSE"));
    QString text;
    if (file.open(QIODevice::ReadOnly))
        text = QString::fromUtf8(file.readAll());
    else
        text = tx("msg.licenseMissing");

    QDialog dialog(this);
    dialog.setWindowTitle(tx("about.licenseTitle"));
    dialog.resize(720, 560);
    auto *view = new QPlainTextEdit(text, &dialog);
    view->setReadOnly(true);
    view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tx("btn.ok"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(view);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::buildCentral()
{
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    // --- top bar ---
    auto *top = new QHBoxLayout;
    auto addTop = [&](const QString &text, auto slot) {
        auto *btn = new QPushButton(text, central);
        btn->setToolTip(text);
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
    autoIndent->setToolTip(tx("btn.autoIndentDoc"));
    connect(autoIndent, &QPushButton::clicked, this, &MainWindow::autoIndentDocument);
    top->addWidget(autoIndent);

    m_minDoc = new QPushButton(tx("btn.minimizeDoc"), central);
    m_minDoc->setToolTip(tx("btn.minimizeDoc"));
    m_minDoc->setCheckable(true);
    connect(m_minDoc, &QPushButton::toggled, this, [this](bool on) {
        if (on) {
            QSignalBlocker other(m_maxDoc);
            m_maxDoc->setChecked(false);
        }
        setAllViews(on ? ViewMode::Minimized : ViewMode::Normal);
    });
    top->addWidget(m_minDoc);

    m_maxDoc = new QPushButton(tx("btn.maximizeDoc"), central);
    m_maxDoc->setToolTip(tx("btn.maximizeDoc"));
    m_maxDoc->setCheckable(true);
    connect(m_maxDoc, &QPushButton::toggled, this, [this](bool on) {
        if (on) {
            QSignalBlocker other(m_minDoc);
            m_minDoc->setChecked(false);
        }
        setAllViews(on ? ViewMode::Maximized : ViewMode::Normal);
    });
    top->addWidget(m_maxDoc);
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
    generate->setToolTip(tx("btn.generate"));
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

void MainWindow::setAllViews(ViewMode mode)
{
    for (TextBlock &b : m_project.blocks)
        for (FtlFile &f : b.files)
            f.view = mode;
    markDirty();
    rebuildBlocks();
}

void MainWindow::syncDocumentViewButtons()
{
    if (!m_minDoc || !m_maxDoc)
        return;
    bool any = false;
    bool allMin = true;
    bool allMax = true;
    for (const TextBlock &b : m_project.blocks)
        for (const FtlFile &f : b.files) {
            any = true;
            allMin = allMin && f.view == ViewMode::Minimized;
            allMax = allMax && f.view == ViewMode::Maximized;
        }
    QSignalBlocker minBlock(m_minDoc);
    QSignalBlocker maxBlock(m_maxDoc);
    m_minDoc->setChecked(any && allMin);
    m_maxDoc->setChecked(any && allMax);
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
        connect(widget, &BlockWidget::moveSelectionRequested, this,
                [this, id](const QString &fileId, int start, int end) {
                    moveSelectionToNewBlock(id, fileId, start, end);
                });
        m_blocksLayout->addWidget(widget);
    }
    m_blocksLayout->addStretch(1);
    syncDocumentViewButtons();
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
    clearUndoHistory();
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
    clearUndoHistory();
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

void MainWindow::moveSelectionToNewBlock(const QString &blockId, const QString &fileId, int start, int end)
{
    const TextBlock *checked = findBlock(m_project, blockId);
    if (!checked || checked->files.size() != 1)
        return;
    const FtlFile &checkedFile = checked->files.first();
    if (checkedFile.id != fileId || start < 0 || end > checkedFile.content.size() || start >= end)
        return;

    QMessageBox box(QMessageBox::Question, tx("ctx.moveToNewBlock"), tx("pos.moveQuestion"), QMessageBox::NoButton, this);
    QPushButton *above = box.addButton(tx("pos.above"), QMessageBox::AcceptRole);
    QPushButton *below = box.addButton(tx("pos.below"), QMessageBox::AcceptRole);
    box.addButton(tx("btn.cancel"), QMessageBox::RejectRole);
    box.exec();

    const bool isAbove = box.clickedButton() == above;
    if (!isAbove && box.clickedButton() != below)
        return;

    recordUndo();
    TextBlock *b = findBlock(m_project, blockId);
    FtlFile &f = b->files.first();
    const int current = blockIndex(m_project, blockId);
    const QString moved = f.content.mid(start, end - start);
    f.content.remove(start, end - start);

    TextBlock fresh = makeBlock(tx("lbl.block").arg(m_project.blocks.size() + 1));
    fresh.files.first().content = moved;
    m_project.blocks.insert(isAbove ? current : current + 1, fresh);

    markDirty();
    rebuildBlocks();
}

void MainWindow::addNewBlock()
{
    const std::optional<int> at = askInsertPosition(tx("btn.newBlock"));
    if (!at)
        return;
    recordUndo();
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
    recordUndo();

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
    case Command::SaveFile:
        saveFile(blockId, fileId, false);
        return;
    case Command::SaveFileAs:
        saveFile(blockId, fileId, true);
        return;
    case Command::AutoIndentFile:
        autoIndentFile(blockId, fileId);
        return;
    }
}

void MainWindow::saveFile(const QString &blockId, const QString &fileId, bool saveAs)
{
    const TextBlock *probeBlock = findBlock(m_project, blockId);
    if (!probeBlock || !findFile(*findBlock(m_project, blockId), fileId))
        return;
    const FtlFile &probe = *findFile(*findBlock(m_project, blockId), fileId);

    QString path = probe.path;
    if (saveAs || path.isEmpty()) {
        const QString suggested = path.isEmpty()
                                      ? QDir(m_lastDir).filePath(sanitizeFileName(probe.label) + QStringLiteral(".ftl"))
                                      : path;
        path = QFileDialog::getSaveFileName(this, tx("dlg.saveAs"), suggested, tx("filter.ftl"));
        if (path.isEmpty())
            return;
        m_lastDir = QFileInfo(path).absolutePath();
    }

    // Content is taken from the model, which is kept in sync with the editor.
    QString error;
    if (!writeTextFile(path, findFile(*findBlock(m_project, blockId), fileId)->content, error)) {
        showError(tx("msg.writeFailed").arg(path, error));
        return;
    }

    FtlFile *f = findFile(*findBlock(m_project, blockId), fileId);
    if (f->path != path) {
        f->path = path;
        markDirty();
        rebuildBlocks(); // the pane shows the file name
    }
    statusBar()->showMessage(tx("msg.saved"), 3000);
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
    if (!findBlock(m_project, blockId))
        return;
    const QStringList paths = QFileDialog::getOpenFileNames(this, tx("dlg.openFtl"), m_lastDir,
                                                            tx("filter.ftl") + QStringLiteral(";;") + tx("filter.all"));
    if (paths.isEmpty())
        return;
    m_lastDir = QFileInfo(paths.first()).absolutePath();

    // Look the block up again after recordUndo(): the snapshot shares data with the
    // project until the first write, so references taken before it must not be used.
    recordUndo();
    TextBlock *b = findBlock(m_project, blockId);
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
    if (!findBlock(m_project, blockId))
        return;
    recordUndo();
    TextBlock *b = findBlock(m_project, blockId);
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
    recordUndo();
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
    recordUndo();
    m_project.blocks.remove(index);
    markDirty();
    rebuildBlocks();
}

void MainWindow::deleteFile(const QString &blockId, const QString &fileId)
{
    const int blockPos = blockIndex(m_project, blockId);
    if (blockPos < 0)
        return;
    const TextBlock &current = m_project.blocks.at(blockPos);
    int fileIndex = -1;
    for (int i = 0; i < current.files.size(); ++i)
        if (current.files.at(i).id == fileId) {
            fileIndex = i;
            break;
        }
    if (fileIndex < 0)
        return;
    const QString label = current.files.at(fileIndex).label;
    if (!current.files.at(fileIndex).content.isEmpty() &&
        !askYesNo(tx("btn.delete"), tx("msg.deleteFile").arg(label)))
        return;

    recordUndo();
    TextBlock &b = m_project.blocks[blockPos];
    b.files.remove(fileIndex);
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
    const QVector<Snapshot> redoBefore = m_redoStack;
    recordUndo();
    bool changed = false;
    for (TextBlock &b : m_project.blocks)
        for (FtlFile &f : b.files)
            changed = indentFile(f) || changed;
    if (!changed) {
        m_undoStack.removeLast();
        m_redoStack = redoBefore;
    } else {
        markDirty();
        rebuildBlocks();
    }
    statusBar()->showMessage(tx("msg.indented"), 3000);
}

void MainWindow::autoIndentBlock(const QString &blockId)
{
    if (!findBlock(m_project, blockId))
        return;
    const QVector<Snapshot> redoBefore = m_redoStack;
    recordUndo();
    TextBlock *b = findBlock(m_project, blockId);
    bool changed = false;
    for (FtlFile &f : b->files)
        changed = indentFile(f) || changed;
    if (!changed) {
        m_undoStack.removeLast();
        m_redoStack = redoBefore;
    } else {
        markDirty();
        rebuildBlocks();
    }
    statusBar()->showMessage(tx("msg.indented"), 3000);
}

void MainWindow::autoIndentFile(const QString &blockId, const QString &fileId)
{
    const TextBlock *probe = findBlock(m_project, blockId);
    if (!probe || !findFile(*findBlock(m_project, blockId), fileId))
        return;
    const QVector<Snapshot> redoBefore = m_redoStack;
    recordUndo();
    FtlFile *f = findFile(*findBlock(m_project, blockId), fileId);
    if (!indentFile(*f)) {
        m_undoStack.removeLast();
        m_redoStack = redoBefore;
    } else {
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

    const QVector<MissingVariant> missing = findMissingVariants(m_project);
    if (!missing.isEmpty()) {
        QStringList lines;
        for (const MissingVariant &m : missing)
            lines << tx("msg.missingVariant").arg(m.blockLabel.isEmpty() ? tx("lbl.noLabel") : m.blockLabel, m.label);
        const QString question = lines.join(QLatin1Char('\n')) + QStringLiteral("\n\n") + tx("msg.continueAnyway");
        if (!askYesNo(tx("btn.generate"), question))
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

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
}

MainWindow::Snapshot MainWindow::currentSnapshot() const
{
    return {m_project.blocks, m_project.collectivePrefix};
}

void MainWindow::recordUndo()
{
    m_undoStack.append(currentSnapshot());
    if (m_undoStack.size() > kMaxUndoSteps)
        m_undoStack.removeFirst();
    m_redoStack.clear();
}

void MainWindow::clearUndoHistory()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

void MainWindow::restoreSnapshot(const Snapshot &snapshot)
{
    m_project.blocks = snapshot.blocks;
    m_project.collectivePrefix = snapshot.prefix;
    markDirty();
    refreshAll();
}

void MainWindow::undo()
{
    if (m_undoStack.isEmpty())
        return;
    m_redoStack.append(currentSnapshot());
    restoreSnapshot(m_undoStack.takeLast());
    statusBar()->showMessage(tx("msg.undone"), 2000);
}

void MainWindow::redo()
{
    if (m_redoStack.isEmpty())
        return;
    m_undoStack.append(currentSnapshot());
    restoreSnapshot(m_redoStack.takeLast());
    statusBar()->showMessage(tx("msg.redone"), 2000);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // A text editor with its own undo history keeps Ctrl+Z / Ctrl+Y for itself.
    // Otherwise the key goes to the block-level undo / redo actions.
    if (event->type() == QEvent::ShortcutOverride) {
        if (auto *editor = qobject_cast<QPlainTextEdit *>(QApplication::focusWidget())) {
            const auto *ke = static_cast<QKeyEvent *>(event);
            const bool editorUndo = ke->matches(QKeySequence::Undo) && editor->document()->isUndoAvailable();
            const bool editorRedo = isRedoKey(ke) && editor->document()->isRedoAvailable();
            if (!editorUndo && !editorRedo && (ke->matches(QKeySequence::Undo) || isRedoKey(ke))) {
                event->ignore();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscardChanges())
        event->accept();
    else
        event->ignore();
}
