#pragma once

#include "commands.h"
#include "document.h"

#include <QFrame>

class QCompleter;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QStringListModel;
class QToolButton;

// One variant file: label editor (with suggestions), min/max buttons and the
// FreeMarker editor. Displays the effective format of the file.
class FilePaneWidget : public QFrame {
    Q_OBJECT
public:
    FilePaneWidget(Project *project, const QString &blockId, const QString &fileId,
                   QWidget *parent = nullptr);

    QString label() const;

signals:
    void commandRequested(const QString &fileId, Command cmd);
    void labelCommitted();   // the label changed; the block must re-sort its panes
    // The user asked to move the selected text [start, end) of this file to a new block.
    void moveSelectionRequested(const QString &fileId, int start, int end);

private:
    FtlFile *file() const;
    void applyEditorFont();
    void applyViewMode(ViewMode mode);   // user action: marks the project dirty
    void updateViewButtons(ViewMode mode);
    void fitEditor();
    void onLabelEdited(const QString &text);
    void commitLabel();
    void showEditorMenu(const QPoint &pos);

    Project *m_project;
    QString m_blockId;
    QString m_fileId;

    QLineEdit *m_labelEdit = nullptr;
    QLabel *m_fileNameLabel = nullptr;
    QToolButton *m_minButton = nullptr;
    QToolButton *m_maxButton = nullptr;
    QPlainTextEdit *m_editor = nullptr;
    QStringListModel *m_suggestions = nullptr;
    QCompleter *m_completer = nullptr;
};
