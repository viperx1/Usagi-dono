#ifndef UICOLORS_H
#define UICOLORS_H

#include <QColor>

// UI Colors for tree widget items and cards
namespace UIColors {
    // Status colors
    inline const QColor FILE_NOT_FOUND = QColor(Qt::red);        // Red for missing/unavailable files
    inline const QColor FILE_AVAILABLE = QColor(0, 150, 0);      // Green for available files
    inline const QColor FILE_WATCHED = QColor(0, 150, 0);        // Green for watched files
    inline const QColor FILE_DELETED = QColor(Qt::black);        // Black for deleted files (state=3)
    
    // File quality colors (for file items background)
    inline const QColor FILE_QUALITY_HIGH = QColor(230, 240, 255);   // Light blue (highest quality)
    inline const QColor FILE_QUALITY_MEDIUM = QColor(255, 250, 230); // Light yellow (medium quality)
    inline const QColor FILE_QUALITY_LOW = QColor(255, 230, 240);    // Light pink (low quality)
    inline const QColor FILE_QUALITY_UNKNOWN = QColor(240, 240, 240); // Light gray (unknown)
    
    // File marking colors
    inline const QColor FILE_MARKED_DOWNLOAD = QColor(100, 180, 255);  // Blue for download queue
}

// File state constants (from mylist.state field)
namespace FileStates {
    inline const QString DELETED = "Deleted";  // File marked as deleted (state=3)
    inline const QString HDD = "HDD";          // File on HDD (state=1)
    inline const QString CD_DVD = "CD/DVD";    // File on CD/DVD (state=2)
    inline const QString UNKNOWN = "Unknown";  // Unknown state (state=0)
}

// AniDB file state bit flags (from file.state field)
namespace AniDBFileStateBits {
    constexpr int FILE_CRCOK = 1;      // Bit 0: CRC32 matches
    constexpr int FILE_CRCERR = 2;     // Bit 1: CRC32 mismatch
    constexpr int FILE_ISV2 = 4;       // Bit 2: File is version 2
    constexpr int FILE_ISV3 = 8;       // Bit 3: File is version 3
    constexpr int FILE_ISV4 = 16;      // Bit 4: File is version 4
    constexpr int FILE_ISV5 = 32;      // Bit 5: File is version 5
    constexpr int FILE_UNC = 64;       // Bit 6: Uncensored
    constexpr int FILE_CEN = 128;      // Bit 7: Censored
}

// UI icons for file marking
namespace UIIcons {
    inline const QString MARK_DOWNLOAD = QString::fromUtf8("\xE2\xAC\x87");   // ⬇ Down arrow
    inline const QString MARK_NONE = "";
}

#endif // UICOLORS_H
