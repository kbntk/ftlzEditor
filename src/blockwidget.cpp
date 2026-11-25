#include "blockwidget.h"

#include "filepanewidget.h"
#include "i18n.h"
#include "labels.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

BlockWidget::BlockWidget(Project *project, const QString &blockId, QWidget *parent)
    : QFrame(parent), m_project(project), m_blockId(blockId)
{
    setFrameShape(QFrame::StyledPanel);
    const TextBlock *b = block();

    auto *root = new QVBoxLayout(this);

    // --- header row ---
    auto *header = new QHBoxLayout;

    m_labelEdit = new QLineEdit(b->label, this);
    m_labelEdit->setMinimumWidth(220);
    connect(m_labelEdit, &QLineEdit::editingFinished, this, [this] {
        TextBlock *blk = block();
        const QString text = m_labelEdit->text().trimmed();
        if (!blk || blk->label == text)
            return;
        blk->label = text;
        emit commandRequested(QString(), Command::Changed);
    });
    header->addWidget(m_labelEdit, 1);

    auto addButton = [this](QBoxLayout *row, const QString &text, Command cmd, const QString &tip = {}) {
        auto *btn = new QPushButton(text, this);
        btn->setToolTip(tip.isEmpty() ? text : tip);
        connect(btn, &QPushButton::clicked, this, [this, cmd] { emit commandRequested(QString(), cmd); });
        row->addWidget(btn);
    };
    addButton(header, tx("btn.save"), Command::SaveBlock);
    addButton(header, tx("btn.saveAs"), Command::SaveBlockAs);
    addButton(header, QStringLiteral("↑"), Command::MoveUp, tx("btn.up"));
    addButton(header, QStringLiteral("↓"), Command::MoveDown, tx("btn.down"));
    addButton(header, tx("btn.autoIndent"), Command::AutoIndentBlock);
    addButton(header, tx("btn.delete"), Command::DeleteBlock);
    root->addLayout(header);

    // --- filter row ---
    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tx("lbl.filter"), this));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setClearButtonEnabled(true);
    for (QToolButton *clear : m_filterEdit->findChildren<QToolButton *>())
        clear->setToolTip(tx("btn.clear"));
    connect(m_filterEdit, &QLineEdit::textChanged, this, &BlockWidget::applyFilter);
    filterRow->addWidget(m_filterEdit, 1);

    m_minAll = new QPushButton(tx("btn.minimize"), this);
    m_minAll->setToolTip(tx("btn.minimizeAll"));
    m_minAll->setCheckable(true);
    connect(m_minAll, &QPushButton::toggled, this, [this](bool on) {
        if (on) {
            QSignalBlocker other(m_maxAll);
            m_maxAll->setChecked(false);
        }
        setAllViews(on ? ViewMode::Minimized : ViewMode::Normal);
    });
    addButton(filterRow, tx("btn.addFromFile"), Command::AddFromFile);
    addButton(filterRow, tx("btn.addEmpty"), Command::AddEmpty);
    filterRow->addWidget(m_minAll);

    m_maxAll = new QPushButton(tx("btn.maximize"), this);
    m_maxAll->setToolTip(tx("btn.maximizeAll"));
    m_maxAll->setCheckable(true);
    connect(m_maxAll, &QPushButton::toggled, this, [this](bool on) {
        if (on) {
            QSignalBlocker other(m_minAll);
            m_minAll->setChecked(false);
        }
        setAllViews(on ? ViewMode::Maximized : ViewMode::Normal);
    });
    filterRow->addWidget(m_maxAll);
    filterRow->addStretch(2);
    syncViewButtons();
    root->addLayout(filterRow);

    // --- variant panes ---
    m_panes = new QWidget(this);
    m_panesLayout = new QHBoxLayout(m_panes);
    m_panesLayout->setContentsMargins(0, 0, 0, 0);
    m_panesLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    root->addWidget(m_panes);

    rebuildPanes();
}

void BlockWidget::setShaded(bool shaded)
{
    setAutoFillBackground(shaded);
    if (shaded) {
        QPalette pal = QGuiApplication::palette();
        pal.setColor(QPalette::Window, pal.color(QPalette::Window).darker(106));
        setPalette(pal);
    } else {
        setPalette(QGuiApplication::palette());
    }
}

TextBlock *BlockWidget::block() const
{
    return findBlock(*m_project, m_blockId);
}

void BlockWidget::rebuildPanes()
{
    while (QLayoutItem *item = m_panesLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }

    const TextBlock *b = block();
    if (!b)
        return;

    QVector<const FtlFile *> files;
    for (const FtlFile &f : b->files)
        files.append(&f);
    std::sort(files.begin(), files.end(), [](const FtlFile *a, const FtlFile *c) {
        return labelLess(a->label, c->label);
    });

    for (const FtlFile *f : files) {
        auto *pane = new FilePaneWidget(m_project, m_blockId, f->id, m_panes);
        connect(pane, &FilePaneWidget::commandRequested, this,
                [this](const QString &fileId, Command cmd) { emit commandRequested(fileId, cmd); });
        connect(pane, &FilePaneWidget::labelCommitted, this, &BlockWidget::rebuildPanes);
        connect(pane, &FilePaneWidget::moveSelectionRequested, this,
                [this](const QString &fileId, int start, int end) { emit moveSelectionRequested(fileId, start, end); });
        m_panesLayout->addWidget(pane);
    }
    applyFilter();
}

void BlockWidget::setAllViews(ViewMode mode)
{
    TextBlock *b = block();
    if (!b)
        return;
    for (FtlFile &f : b->files)
        f.view = mode;
    syncViewButtons();
    emit commandRequested(QString(), Command::Changed);
    rebuildPanes(); // editors take their new height from the model
}

void BlockWidget::syncViewButtons()
{
    const TextBlock *b = block();
    if (!b)
        return;
    bool allMin = !b->files.isEmpty();
    bool allMax = !b->files.isEmpty();
    for (const FtlFile &f : b->files) {
        allMin = allMin && f.view == ViewMode::Minimized;
        allMax = allMax && f.view == ViewMode::Maximized;
    }
    QSignalBlocker minBlock(m_minAll);
    QSignalBlocker maxBlock(m_maxAll);
    m_minAll->setChecked(allMin);
    m_maxAll->setChecked(allMax);
}

void BlockWidget::applyFilter()
{
    const QString filter = m_filterEdit->text();
    for (FilePaneWidget *pane : m_panes->findChildren<FilePaneWidget *>(QString(), Qt::FindDirectChildrenOnly))
        pane->setVisible(labelMatchesFilter(pane->label(), filter));
}
