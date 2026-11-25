#include "filepanewidget.h"

#include "ftlhighlighter.h"
#include "i18n.h"
#include "labels.h"

#include <QCompleter>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kColumns = 120;     // editor width in characters
constexpr int kNormalLines = 20;  // editor height in normal mode
} // namespace

FilePaneWidget::FilePaneWidget(Project *project, const QString &blockId, const QString &fileId,
                               QWidget *parent)
    : QFrame(parent), m_project(project), m_blockId(blockId), m_fileId(fileId)
{
    setFrameShape(QFrame::StyledPanel);
    const FtlFile *f = file();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    // --- header: label, file name, view buttons, format, delete ---
    auto *header = new QHBoxLayout;

    m_labelEdit = new QLineEdit(f->label, this);
    m_labelEdit->setPlaceholderText(tx("lbl.fileLabel"));
    m_labelEdit->setMinimumWidth(180);
    m_suggestions = new QStringListModel(this);
    m_completer = new QCompleter(m_suggestions, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_labelEdit->setCompleter(m_completer);
    connect(m_labelEdit, &QLineEdit::textEdited, this, &FilePaneWidget::onLabelEdited);
    connect(m_labelEdit, &QLineEdit::editingFinished, this, &FilePaneWidget::commitLabel);
    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated), this,
            [this](const QString &text) {
                m_labelEdit->setText(text);
                commitLabel();
            });
    header->addWidget(m_labelEdit, 1);

    const QString fileName = f->path.isEmpty() ? tx("lbl.notSaved") : QFileInfo(f->path).fileName();
    m_fileNameLabel = new QLabel(fileName, this);
    m_fileNameLabel->setToolTip(f->path);
    header->addWidget(m_fileNameLabel);

    m_minButton = new QToolButton(this);
    m_minButton->setText(tx("btn.minimize"));
    m_minButton->setCheckable(true);
    connect(m_minButton, &QToolButton::toggled, this, [this](bool on) {
        applyViewMode(on ? ViewMode::Minimized : ViewMode::Normal);
    });
    header->addWidget(m_minButton);

    m_maxButton = new QToolButton(this);
    m_maxButton->setText(tx("btn.maximize"));
    m_maxButton->setCheckable(true);
    connect(m_maxButton, &QToolButton::toggled, this, [this](bool on) {
        applyViewMode(on ? ViewMode::Maximized : ViewMode::Normal);
    });
    header->addWidget(m_maxButton);

    auto *indentButton = new QToolButton(this);
    indentButton->setText(tx("btn.autoIndent"));
    connect(indentButton, &QToolButton::clicked, this,
            [this] { emit commandRequested(m_fileId, Command::AutoIndentFile); });
    header->addWidget(indentButton);

    auto *deleteButton = new QToolButton(this);
    deleteButton->setText(tx("btn.delete"));
    connect(deleteButton, &QToolButton::clicked, this,
            [this] { emit commandRequested(m_fileId, Command::DeleteFile); });
    header->addWidget(deleteButton);

    layout->addLayout(header);

    // --- editor ---
    m_editor = new QPlainTextEdit(this);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setPlainText(f->content);
    new FtlHighlighter(m_editor->document());
    connect(m_editor, &QPlainTextEdit::textChanged, this, [this] {
        if (FtlFile *fl = file()) {
            fl->content = m_editor->toPlainText();
            if (fl->view == ViewMode::Maximized)
                fitEditor();
            emit commandRequested(m_fileId, Command::Changed);
        }
    });
    layout->addWidget(m_editor);

    applyEditorFont();
    updateViewButtons(f->view); // initial state: must not mark the project dirty
    fitEditor();
}

QString FilePaneWidget::label() const
{
    const FtlFile *f = file();
    return f ? f->label : QString();
}

FtlFile *FilePaneWidget::file() const
{
    TextBlock *b = findBlock(*m_project, m_blockId);
    return b ? findFile(*b, m_fileId) : nullptr;
}

void FilePaneWidget::applyEditorFont()
{
    const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_editor->setFont(font);
    m_editor->setTabStopDistance(QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * 4);
    fitEditor();
}

void FilePaneWidget::updateViewButtons(ViewMode mode)
{
    QSignalBlocker minBlock(m_minButton);
    QSignalBlocker maxBlock(m_maxButton);
    m_minButton->setChecked(mode == ViewMode::Minimized);
    m_maxButton->setChecked(mode == ViewMode::Maximized);
}

void FilePaneWidget::applyViewMode(ViewMode mode)
{
    FtlFile *f = file();
    if (!f)
        return;

    f->view = mode;
    updateViewButtons(mode);
    fitEditor();
    emit commandRequested(m_fileId, Command::Changed);
}

void FilePaneWidget::fitEditor()
{
    const FtlFile *f = file();
    const ViewMode mode = f ? f->view : ViewMode::Normal;

    const QFontMetricsF fm(m_editor->font());
    const qreal frame = 2.0 * m_editor->frameWidth();
    const qreal docMargin = 2.0 * m_editor->document()->documentMargin();
    const int scrollBarWidth = m_editor->verticalScrollBar()->sizeHint().width();
    const int scrollBarHeight = m_editor->horizontalScrollBar()->sizeHint().height();

    const qreal width = fm.horizontalAdvance(QString(kColumns, QLatin1Char('0')));
    m_editor->setFixedWidth(static_cast<int>(std::ceil(width + docMargin + frame + scrollBarWidth + 4)));

    int lines = kNormalLines;
    if (mode == ViewMode::Minimized)
        lines = 1;
    else if (mode == ViewMode::Maximized)
        lines = std::max(1, m_editor->document()->blockCount());

    m_editor->setFixedHeight(static_cast<int>(
        std::ceil(fm.lineSpacing() * lines + docMargin + frame + scrollBarHeight + 4)));
}

void FilePaneWidget::onLabelEdited(const QString &text)
{
    const QStringList suggestions = suggestLabels(text, allFileLabels(*m_project));
    m_suggestions->setStringList(suggestions);
    if (!suggestions.isEmpty()) {
        m_completer->setCompletionPrefix(text);
        m_completer->complete();
    }
}

void FilePaneWidget::commitLabel()
{
    FtlFile *f = file();
    if (!f)
        return;
    const QString text = m_labelEdit->text().trimmed();
    if (text == f->label)
        return;
    f->label = text;
    emit commandRequested(m_fileId, Command::Changed);
    emit labelCommitted();
}
