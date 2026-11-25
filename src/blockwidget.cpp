#include "blockwidget.h"

#include "filepanewidget.h"
#include "i18n.h"
#include "labels.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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
    header->addWidget(new QLabel(b->isShared() ? tx("tag.shared") : tx("tag.variants"), this));

    auto addButton = [this, header](const QString &text, Command cmd, const QString &tip = {}) {
        auto *btn = new QPushButton(text, this);
        if (!tip.isEmpty())
            btn->setToolTip(tip);
        connect(btn, &QPushButton::clicked, this, [this, cmd] { emit commandRequested(QString(), cmd); });
        header->addWidget(btn);
    };
    addButton(tx("btn.save"), Command::SaveBlock);
    addButton(tx("btn.saveAs"), Command::SaveBlockAs);
    addButton(tx("btn.addFromFile"), Command::AddFromFile);
    addButton(tx("btn.addEmpty"), Command::AddEmpty);
    addButton(QStringLiteral("↑"), Command::MoveUp, tx("btn.up"));
    addButton(QStringLiteral("↓"), Command::MoveDown, tx("btn.down"));
    addButton(tx("btn.autoIndent"), Command::AutoIndentBlock);
    addButton(tx("btn.delete"), Command::DeleteBlock);
    root->addLayout(header);

    // --- filter row ---
    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tx("lbl.filter"), this));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &BlockWidget::applyFilter);
    filterRow->addWidget(m_filterEdit, 1);
    filterRow->addStretch(2);
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

void BlockWidget::applyFilter()
{
    const QString filter = m_filterEdit->text();
    for (FilePaneWidget *pane : m_panes->findChildren<FilePaneWidget *>(QString(), Qt::FindDirectChildrenOnly))
        pane->setVisible(labelMatchesFilter(pane->label(), filter));
}
