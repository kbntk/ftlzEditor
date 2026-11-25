#pragma once

// Actions a block or a variant pane asks the main window to perform.
enum class Command {
    Changed,       // content or label changed (marks project dirty)
    SaveBlock,
    SaveBlockAs,
    AddFromFile,   // add variant(s) from disk
    AddEmpty,      // add an empty variant
    MoveUp,
    MoveDown,
    DeleteBlock,
    AutoIndentBlock,  // re-indent all variants of the block
    DeleteFile,    // delete one variant (fileId required)
    AutoIndentFile, // re-indent one variant (fileId required)
    SaveFile,       // save one variant to its file (fileId required)
    SaveFileAs,     // save one variant to a chosen file (fileId required)
};
