#ifndef ANIMEUTILS_H
#define ANIMEUTILS_H

#include <QString>

/**
 * Utility functions for anime data processing
 */
namespace AnimeUtils {

/**
 * Determine the best anime name from available options
 * Priority: nameRomaji > nameEnglish > animeTitle > fallback to "Anime #<aid>"
 * 
 * @param nameRomaji Romaji name from anime table
 * @param nameEnglish English name from anime table
 * @param animeTitle Title from anime_titles table
 * @param aid Anime ID for fallback
 * @return Best available anime name
 */
inline QString determineAnimeName(const QString& nameRomaji, const QString& nameEnglish, const QString& animeTitle, int aid)
{
    QString animeName = nameRomaji;
    
    if (animeName.isEmpty() && !nameEnglish.isEmpty()) {
        animeName = nameEnglish;
    }
    if (animeName.isEmpty() && !animeTitle.isEmpty()) {
        animeName = animeTitle;
    }
    if (animeName.isEmpty()) {
        animeName = QString("Anime #%1").arg(aid);
    }
    
    return animeName;
}

/**
 * Extract file version from AniDB file state bits
 * 
 * State field bit encoding (from AniDB UDP API):
 *   Bit 0 (1): FILE_CRCOK
 *   Bit 1 (2): FILE_CRCERR
 *   Bit 2 (4): FILE_ISV2 - file is version 2
 *   Bit 3 (8): FILE_ISV3 - file is version 3
 *   Bit 4 (16): FILE_ISV4 - file is version 4
 *   Bit 5 (32): FILE_ISV5 - file is version 5
 *   Bit 6 (64): FILE_UNC - uncensored
 *   Bit 7 (128): FILE_CEN - censored
 * 
 * If no version bits are set, the file is version 1
 * 
 * @param state AniDB file state field value
 * @return File version (1-5)
 */
inline int extractFileVersion(int state)
{
    // Check version flags in priority order (v5 > v4 > v3 > v2)
    if (state & 32) {      // Bit 5: FILE_ISV5
        return 5;
    } else if (state & 16) { // Bit 4: FILE_ISV4
        return 4;
    } else if (state & 8) {  // Bit 3: FILE_ISV3
        return 3;
    } else if (state & 4) {  // Bit 2: FILE_ISV2
        return 2;
    }
    return 1;  // No version bits set means version 1
}

} // namespace AnimeUtils

#endif // ANIMEUTILS_H
