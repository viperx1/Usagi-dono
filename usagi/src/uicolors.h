#ifndef UICOLORS_H
#define UICOLORS_H

#include <QColor>

// UI Colors for tree widget items and cards
namespace UIColors {
    // Status colors
    inline const QColor FILE_NOT_FOUND = QColor(Qt::red);        // Red for missing/unavailable files
    inline const QColor FILE_AVAILABLE = QColor(0, 150, 0);      // Green for available files
    inline const QColor FILE_WATCHED = QColor(0, 150, 0);        // Green for watched files
    
    // File quality colors (for file items background)
    inline const QColor FILE_QUALITY_HIGH = QColor(230, 240, 255);   // Light blue (highest quality)
    inline const QColor FILE_QUALITY_MEDIUM = QColor(255, 250, 230); // Light yellow (medium quality)
    inline const QColor FILE_QUALITY_LOW = QColor(255, 230, 240);    // Light pink (low quality)
    inline const QColor FILE_QUALITY_UNKNOWN = QColor(240, 240, 240); // Light gray (unknown)
}

#endif // UICOLORS_H
