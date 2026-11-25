#pragma once

#include "commands.h"
#include "document.h"

#include <QMainWindow>

#include <optional>

class QEvent;
class QLabel;
class QMenu;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildMenus();
    void buildCentral();
    void showAbout();
    void showLicense();
    void showOptions();
    void populateRecentMenu();
    void openRecentProject(const QString &path);
    bool loadProject(const QString &path);
    void refreshAll();          // reload the central UI from m_project
    void rebuildBlocks();
    void setLanguage(int languageIndex);
    void markDirty();
    void updateTitle();
    void showError(const QString &message);
    bool askYesNo(const QString &title, const QString &question);

    bool confirmDiscardChanges();   // true = continue
    void newProject();
    void openProject();
    bool saveProject();
    bool saveProjectAs();
    void openFtlAsBlocks();
    void addNewBlock();
    void moveSelectionToNewBlock(const QString &blockId, const QString &fileId, int start, int end);
    std::optional<int> askInsertPosition(const QString &title);   // empty = cancelled

    void handleCommand(const QString &blockId, const QString &fileId, Command cmd);
    void saveBlock(const QString &blockId, bool saveAs);
    void saveFile(const QString &blockId, const QString &fileId, bool saveAs);
    void addVariantsFromFiles(const QString &blockId);
    void addEmptyVariant(const QString &blockId);
    void moveBlock(const QString &blockId, int delta);
    void deleteBlock(const QString &blockId);
    void deleteFile(const QString &blockId, const QString &fileId);
    void autoIndentDocument();
    void autoIndentBlock(const QString &blockId);
    void autoIndentFile(const QString &blockId, const QString &fileId);
    void generateFiles();
    void setAllViews(ViewMode mode);            // every file of the document
    void syncDocumentViewButtons();

    // Undo / redo of block-level changes (text typed in an editor uses the editor's own undo).
    struct Snapshot {
        QVector<TextBlock> blocks;
        QString prefix;
    };
    void recordUndo();          // call right before a change to the blocks
    void clearUndoHistory();
    void undo();
    void redo();
    void restoreSnapshot(const Snapshot &snapshot);
    Snapshot currentSnapshot() const;

    QVector<Snapshot> m_undoStack;
    QVector<Snapshot> m_redoStack;

    Project m_project;
    QString m_lastDir;

    QWidget *m_blocksHost = nullptr;
    QVBoxLayout *m_blocksLayout = nullptr;
    QLineEdit *m_prefixEdit = nullptr;
    QPushButton *m_minDoc = nullptr;
    QPushButton *m_maxDoc = nullptr;
    QMenu *m_recentMenu = nullptr;
};
