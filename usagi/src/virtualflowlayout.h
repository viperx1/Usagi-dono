#ifndef VIRTUALFLOWLAYOUT_H
#define VIRTUALFLOWLAYOUT_H

#include <QWidget>
#include <QScrollArea>
#include <QRect>
#include <QMap>
#include <QList>
#include <QTimer>
#include <functional>

// Forward declaration for overlay
class ArrowOverlay;

/**
 * VirtualFlowLayout - A virtual scrolling flow layout for efficiently displaying many cards
 * 
 * This layout only creates and renders widgets that are visible in the viewport,
 * significantly reducing memory usage and improving performance when displaying
 * hundreds or thousands of cards.
 * 
 * Design principles:
 * - Only visible items have widgets created
 * - Widgets are recycled when scrolling (moved to new positions)
 * - Layout calculations are done for all items but only visible ones get widgets
 * - Uses a callback to create widgets on demand
 */
class VirtualFlowLayout : public QWidget
{
    Q_OBJECT
    
public:
    explicit VirtualFlowLayout(QWidget *parent = nullptr);
    virtual ~VirtualFlowLayout();
    
    // Item factory callback type: takes item index and returns a QWidget
    using ItemFactory = std::function<QWidget*(int index)>;
    
    // Set the item factory callback
    void setItemFactory(const ItemFactory &factory);
    
    // Set the number of items in the layout
    void setItemCount(int count);
    
    // Set the fixed size for all items
    void setItemSize(QSize size);
    
    // Set spacing between items
    void setSpacing(int horizontal, int vertical);
    
    // Get the number of items
    int itemCount() const { return m_itemCount; }
    
    // Get item size
    QSize itemSize() const { return m_itemSize; }
    
    // Get a widget at a given index (may be null if not currently visible)
    QWidget* widgetAt(int index) const;
    
    // Get the index for a widget (returns -1 if widget is not managed by this layout)
    int indexOfWidget(QWidget *widget) const;
    
    // Force a specific widget to be visible (scrolls to it if needed)
    void ensureVisible(int index);
    
    // Force a relayout/refresh of visible items
    void refresh();
    
    // Clear all items
    void clear();
    
    // Update a specific item (recreates its widget if visible)
    void updateItem(int index);
    
    // Get the scroll area this layout is contained in
    QScrollArea* scrollArea() const { return m_scrollArea; }
    
    // Set the scroll area (call after adding this widget to a scroll area)
    void setScrollArea(QScrollArea *scrollArea);
    
    // Get total content height
    int contentHeight() const;
    
    // Get visible widgets (for arrow overlay)
    const QMap<int, QWidget*>& getVisibleWidgets() const { return m_visibleWidgets; }
    
signals:
    // Emitted when a visible item's widget is created
    void widgetCreated(int index, QWidget *widget);
    
    // Emitted when a widget is about to be recycled (reused for another item)
    void widgetRecycled(int oldIndex, int newIndex, QWidget *widget);
    
protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    
private slots:
    void onScrollChanged();
    
private:
    // Calculate layout positions for all items (does not create widgets)
    void calculateLayout();
    
    // Update which items should be visible based on scroll position
    void updateVisibleItems();
    
    // Get the visible rect in this widget's coordinates
    QRect visibleRect() const;
    
    // Get the row for a given Y position
    int rowAtY(int y) const;
    
    // Get the first and last visible item indices
    void getVisibleRange(int &firstVisible, int &lastVisible) const;
    
    // Create or reuse a widget for the given index
    QWidget* createOrReuseWidget(int index);
    
    // Return a widget to the pool
    void recycleWidget(int index);
    
    // Scroll position preservation helpers - used during layout updates
    // to prevent unwanted scroll position changes
    int saveScrollPosition() const;       // Returns current vertical scroll position
    void restoreScrollPosition(int savedY); // Restores scroll position to savedY
    
    // Item data
    int m_itemCount;
    QSize m_itemSize;
    int m_hSpacing;
    int m_vSpacing;
    
    // Layout calculations
    int m_columnsPerRow;
    int m_rowHeight;
    int m_totalRows;
    int m_contentHeight;
    
    // Widget management
    QMap<int, QWidget*> m_visibleWidgets;  // index -> widget for currently visible items
    
    // Callback
    ItemFactory m_itemFactory;
    
    // Scroll area reference
    QScrollArea *m_scrollArea;
    
    // Visible range caching
    int m_cachedFirstVisible;
    int m_cachedLastVisible;
    
    // Arrow overlay widget for painting on top of all children
    ArrowOverlay *m_arrowOverlay;
    
    // Deferred update timer to handle initialization timing
    QTimer *m_deferredUpdateTimer;
    
    // Re-entrancy guard for layout operations (calculateLayout and updateVisibleItems)
    bool m_inLayoutUpdate = false;
    
    // Constants
    static constexpr int BUFFER_ROWS = 2;  // Extra rows to render above/below viewport
};

#endif // VIRTUALFLOWLAYOUT_H
