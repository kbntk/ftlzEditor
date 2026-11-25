#pragma once

#include "commands.h"
#include "document.h"

#include <QFrame>

class QHBoxLayout;
class QLineEdit;
class QPushButton;

// One text block: header (label, save, variants, order, format, delete,
// label filter) and the variant panes side by side, sorted by label.
class BlockWidget : public QFrame {
    Q_OBJECT
public:
    BlockWidget(Project *project, const QString &blockId, QWidget *parent = nullptr);

    QString blockId() const { return m_blockId; }
    // Recreates only the variant panes, e.g. after a variant was added or removed.
    void refreshPanes() { rebuildPanes(); }

    // Slightly darkens the background; used for every second block.
    void setShaded(bool shaded);

signals:
    // fileId is empty for block-level commands.
    void commandRequested(const QString &fileId, Command cmd);
    void moveSelectionRequested(const QString &fileId, int start, int end);

private:
    TextBlock *block() const;
    void rebuildPanes();
    void applyFilter();
    void setAllViews(ViewMode mode);   // minimize / maximize / normal for every file
    void syncViewButtons();

    Project *m_project;
    QString m_blockId;
    QLineEdit *m_labelEdit = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QPushButton *m_minAll = nullptr;
    QPushButton *m_maxAll = nullptr;
    QWidget *m_panes = nullptr;
    QHBoxLayout *m_panesLayout = nullptr;
};
