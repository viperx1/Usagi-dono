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

} // namespace AnimeUtils

#endif // ANIMEUTILS_H
