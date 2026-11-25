#pragma once

#include "commands.h"
#include "document.h"

#include <QFrame>

class QHBoxLayout;
class QLineEdit;

// One text block: header (label, save, variants, order, format, delete,
// label filter) and the variant panes side by side, sorted by label.
class BlockWidget : public QFrame {
    Q_OBJECT
public:
    BlockWidget(Project *project, const QString &blockId, QWidget *parent = nullptr);

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

    Project *m_project;
    QString m_blockId;
    QLineEdit *m_labelEdit = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QWidget *m_panes = nullptr;
    QHBoxLayout *m_panesLayout = nullptr;
};
