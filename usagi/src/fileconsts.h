#ifndef FILECONSTS_H
#define FILECONSTS_H

#include <QString>

/**
 * @file fileconsts.h
 * @brief Constants related to file states and AniDB file information
 * 
 * This header contains constants for:
 * - MyList file state strings (HDD, Deleted, etc.)
 * - AniDB file state bit flags (version, CRC, censorship, etc.)
 */

/**
 * @namespace FileStates
 * @brief MyList file state string constants
 * 
 * These strings represent the state field from the mylist table.
 * They are derived from numeric state values:
 *   0 = Unknown
 *   1 = HDD (on hard drive)
 *   2 = CD/DVD
 *   3 = Deleted
 */
namespace FileStates {
    inline const QString DELETED = "Deleted";  // File marked as deleted (state=3)
    inline const QString HDD = "HDD";          // File on HDD (state=1)
    inline const QString CD_DVD = "CD/DVD";    // File on CD/DVD (state=2)
    inline const QString UNKNOWN = "Unknown";  // Unknown state (state=0)
}

/**
 * @namespace AniDBFileStateBits
 * @brief AniDB file state bit flags
 * 
 * These constants represent bit flags in the file.state field from AniDB.
 * The state field is a bitfield where each bit has a specific meaning.
 * 
 * Version bits (2-5) are mutually exclusive:
 * - No version bits set = version 1
 * - Only one version bit should be set at a time
 * 
 * CRC bits (0-1) are mutually exclusive:
 * - Bit 0: CRC matches (good file)
 * - Bit 1: CRC mismatch (corrupted/modified file)
 * 
 * Censorship bits (6-7) are mutually exclusive:
 * - Bit 6: Uncensored version
 * - Bit 7: Censored version
 */
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

#endif // FILECONSTS_H
