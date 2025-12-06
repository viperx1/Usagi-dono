#include "treewidgetsortutil.h"

bool TreeWidgetSortUtil::compareByPlayState(const QTreeWidgetItem* thisItem,
                                            const QTreeWidgetItem* otherItem,
                                            int column)
{
    int thisSortKey = thisItem->data(column, Qt::UserRole).toInt();
    int otherSortKey = otherItem->data(column, Qt::UserRole).toInt();
    return thisSortKey < otherSortKey;
}

bool TreeWidgetSortUtil::compareByLastPlayedTimestamp(const QTreeWidgetItem* thisItem,
                                                      const QTreeWidgetItem* otherItem,
                                                      int column)
{
    qint64 thisTimestamp = thisItem->data(column, Qt::UserRole).toLongLong();
    qint64 otherTimestamp = otherItem->data(column, Qt::UserRole).toLongLong();
    
    // Entries with timestamp 0 (never played) should always be at the bottom
    // regardless of sort order (ascending or descending)
    Qt::SortOrder order = thisItem->treeWidget()->header()->sortIndicatorOrder();
    
    if (thisTimestamp == 0 && otherTimestamp == 0) {
        return false; // Both never played, keep current order
    }
    if (thisTimestamp == 0) {
        // This is never played - should be at bottom
        // In ascending order, return false (not less than, so comes after)
        // In descending order, return true (less than, so comes after when reversed)
        return (order == Qt::DescendingOrder);
    }
    if (otherTimestamp == 0) {
        // Other is never played - should be at bottom
        // In ascending order, return true (this is less than, so comes before)
        // In descending order, return false (not less than, so comes before when reversed)
        return (order == Qt::AscendingOrder);
    }
    
    return thisTimestamp < otherTimestamp;
}
