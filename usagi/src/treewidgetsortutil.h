#ifndef TREEWIDGETSORTUTIL_H
#define TREEWIDGETSORTUTIL_H

#include <QtWidgets/QTreeWidgetItem>
#include <QtWidgets/QHeaderView>
#include <QtCore/QtGlobal>

/**
 * @brief Utility class for common QTreeWidgetItem sorting operations
 * 
 * This class encapsulates duplicate sorting logic that was previously repeated
 * in EpisodeTreeWidgetItem, AnimeTreeWidgetItem, and FileTreeWidgetItem.
 * Following SOLID principles, this extracts common behavior into a reusable utility.
 */
class TreeWidgetSortUtil
{
public:
    /**
     * @brief Compare two items based on play state stored in UserRole
     * @param thisItem The first item to compare
     * @param otherItem The second item to compare
     * @param column The column index (should be COL_PLAY)
     * @return true if thisItem should come before otherItem
     */
    static bool compareByPlayState(const QTreeWidgetItem* thisItem, 
                                   const QTreeWidgetItem* otherItem, 
                                   int column);

    /**
     * @brief Compare two items based on last played timestamp
     * 
     * Handles special case where timestamp 0 (never played) should always
     * be at the bottom regardless of sort order.
     * 
     * @param thisItem The first item to compare
     * @param otherItem The second item to compare
     * @param column The column index (should be COL_LAST_PLAYED)
     * @return true if thisItem should come before otherItem
     */
    static bool compareByLastPlayedTimestamp(const QTreeWidgetItem* thisItem, 
                                             const QTreeWidgetItem* otherItem, 
                                             int column);
};

#endif // TREEWIDGETSORTUTIL_H
