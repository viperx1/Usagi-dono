#include "virtualflowlayout.h"
#include "animecard.h"  // For accessing AnimeCard methods
#include "logger.h"
#include <QScrollBar>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScopeGuard>
#include <algorithm>
#include <cmath>  // For M_PI

// ArrowOverlay - A transparent widget that sits on top of everything to draw arrows
class ArrowOverlay : public QWidget
{
public:
    explicit ArrowOverlay(VirtualFlowLayout *parent) 
        : QWidget(parent)
        , m_layout(parent)
    {
        // Make this widget transparent to mouse events so cards underneath remain interactive
        setAttribute(Qt::WA_TransparentForMouseEvents);
        // Ensure this widget is always on top
        raise();
        // Make background transparent
        setAttribute(Qt::WA_TranslucentBackground);
        // Don't let this widget steal focus
        setFocusPolicy(Qt::NoFocus);
    }
    
    void updateGeometry()
    {
        // Match parent's geometry
        if (m_layout) {
            setGeometry(m_layout->rect());
            raise();  // Ensure we stay on top
        }
    }
    
protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        
        if (!m_layout) {
            return;
        }
        
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // Arrow appearance constants
        const int arrowLineThickness = 3;
        const int arrowHeadSize = 10;
        const QColor arrowColor(0, 120, 215);  // Blue color for visibility
        
        painter.setPen(QPen(arrowColor, arrowLineThickness));
        painter.setBrush(arrowColor);
        
        // Get visible widgets from the layout
        const QMap<int, QWidget*>& visibleWidgets = m_layout->getVisibleWidgets();
        
        // Iterate through visible widgets and draw arrows where appropriate
        for (auto it = visibleWidgets.constBegin(); it != visibleWidgets.constEnd(); ++it) {
            AnimeCard* card = qobject_cast<AnimeCard*>(it.value());
            if (!card || card->isHidden()) {
                continue;
            }
            
            int sequelAid = card->getSequelAid();
            if (sequelAid == 0) {
                continue;  // No sequel, no arrow to draw
            }
            
            // Find the sequel card in visible widgets
            AnimeCard* sequelCard = nullptr;
            for (auto seqIt = visibleWidgets.constBegin(); seqIt != visibleWidgets.constEnd(); ++seqIt) {
                AnimeCard* candidateCard = qobject_cast<AnimeCard*>(seqIt.value());
                if (candidateCard && candidateCard->getAnimeId() == sequelAid) {
                    sequelCard = candidateCard;
                    break;
                }
            }
            
            if (!sequelCard || sequelCard->isHidden()) {
                continue;  // Sequel card not visible or hidden
            }
            
            // Get connection points in parent's coordinates
            QPoint startPoint = card->getRightConnectionPoint();
            QPoint endPoint = sequelCard->getLeftConnectionPoint();
            
            // Convert to this overlay's coordinate system (same as parent)
            startPoint = mapFromGlobal(startPoint);
            endPoint = mapFromGlobal(endPoint);
            
            // Draw arrow line
            painter.drawLine(startPoint, endPoint);
            
            // Draw arrow head at the end point
            QLineF line(startPoint, endPoint);
            double angle = line.angle() * M_PI / 180.0;
            
            QPointF arrowP1 = endPoint - QPointF(arrowHeadSize * cos(angle - M_PI / 6),
                                                  -arrowHeadSize * sin(angle - M_PI / 6));
            QPointF arrowP2 = endPoint - QPointF(arrowHeadSize * cos(angle + M_PI / 6),
                                                  -arrowHeadSize * sin(angle + M_PI / 6));
            
            QPolygonF arrowHead;
            arrowHead << endPoint << arrowP1 << arrowP2;
            painter.drawPolygon(arrowHead);
        }
    }
    
private:
    VirtualFlowLayout *m_layout;
};

VirtualFlowLayout::VirtualFlowLayout(QWidget *parent)
    : QWidget(parent)
    , m_itemCount(0)
    , m_itemSize(600, 450)  // Default anime card size
    , m_hSpacing(10)
    , m_vSpacing(10)
    , m_columnsPerRow(1)
    , m_rowHeight(460)
    , m_totalRows(0)
    , m_contentHeight(0)
    , m_scrollArea(nullptr)
    , m_cachedFirstVisible(-1)
    , m_cachedLastVisible(-1)
    , m_arrowOverlay(nullptr)
    , m_deferredUpdateTimer(nullptr)
    , m_inLayoutUpdate(false)
{
    setMinimumSize(m_itemSize);
    
    // Create arrow overlay that will paint on top of all children
    m_arrowOverlay = new ArrowOverlay(this);
    m_arrowOverlay->show();
    m_arrowOverlay->updateGeometry();
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Create deferred update timer for handling initialization timing issues
    m_deferredUpdateTimer = new QTimer(this);
    m_deferredUpdateTimer->setSingleShot(true);
    m_deferredUpdateTimer->setInterval(100);  // 100ms delay
    connect(m_deferredUpdateTimer, &QTimer::timeout, this, [this]() {
        int savedScrollY = saveScrollPosition();
        
        m_cachedFirstVisible = -1;
        m_cachedLastVisible = -1;
        // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
        calculateLayout();
        updateVisibleItems();
        update();
        
        restoreScrollPosition(savedScrollY);
    });
}

VirtualFlowLayout::~VirtualFlowLayout()
{
    clear();
}

void VirtualFlowLayout::setItemFactory(const ItemFactory &factory)
{
    m_itemFactory = factory;
}

void VirtualFlowLayout::setItemCount(int count)
{
    if (m_itemCount == count) {
        return;
    }
    
    int savedScrollY = saveScrollPosition();
    
    // Clear existing widgets if count decreased
    if (count < m_itemCount) {
        // Recycle widgets that are beyond the new count
        QList<int> indicesToRemove;
        for (auto it = m_visibleWidgets.begin(); it != m_visibleWidgets.end(); ++it) {
            if (it.key() >= count) {
                indicesToRemove.append(it.key());
            }
        }
        for (int idx : indicesToRemove) {
            recycleWidget(idx);
        }
    }
    
    m_itemCount = count;
    
    // Reset cached visible range to force recalculation
    m_cachedFirstVisible = -1;
    m_cachedLastVisible = -1;
    
    // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
    calculateLayout();
    updateVisibleItems();
    update();
    
    restoreScrollPosition(savedScrollY);
    
    // Also schedule a deferred update to catch any timing issues
    if (count > 0 && m_deferredUpdateTimer) {
        m_deferredUpdateTimer->start();
    }
}

void VirtualFlowLayout::setItemSize(QSize size)
{
    if (m_itemSize == size) {
        return;
    }
    
    m_itemSize = size;
    m_rowHeight = size.height() + m_vSpacing;
    // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
    calculateLayout();
    updateVisibleItems();
    update();
}

void VirtualFlowLayout::setSpacing(int horizontal, int vertical)
{
    if (m_hSpacing == horizontal && m_vSpacing == vertical) {
        return;
    }
    
    m_hSpacing = horizontal;
    m_vSpacing = vertical;
    m_rowHeight = m_itemSize.height() + m_vSpacing;
    // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
    calculateLayout();
    updateVisibleItems();
    update();
}

QWidget* VirtualFlowLayout::widgetAt(int index) const
{
    return m_visibleWidgets.value(index, nullptr);
}

int VirtualFlowLayout::indexOfWidget(QWidget *widget) const
{
    for (auto it = m_visibleWidgets.constBegin(); it != m_visibleWidgets.constEnd(); ++it) {
        if (it.value() == widget) {
            return it.key();
        }
    }
    return -1;
}

void VirtualFlowLayout::ensureVisible(int index)
{
    if (!m_scrollArea || index < 0 || index >= m_itemCount) {
        return;
    }
    
    // Calculate position of the item
    int row = index / m_columnsPerRow;
    int y = row * m_rowHeight;
    
    m_scrollArea->ensureVisible(0, y + m_itemSize.height() / 2, 0, m_itemSize.height() / 2 + m_vSpacing);
}

void VirtualFlowLayout::refresh()
{
    LOG("[VirtualFlowLayout] refresh() started");
    int savedScrollY = saveScrollPosition();
    LOG(QString("[VirtualFlowLayout] Saved scroll position: %1").arg(savedScrollY));
    
    // Force recalculation and update of all visible items
    // We need to hide and clear all visible widgets first because after
    // sorting/filtering, the same index may now represent a different item
    LOG(QString("[VirtualFlowLayout] Hiding %1 visible widgets").arg(m_visibleWidgets.size()));
    for (auto it = m_visibleWidgets.begin(); it != m_visibleWidgets.end(); ++it) {
        if (it.value()) {
            it.value()->hide();
        }
    }
    m_visibleWidgets.clear();
    LOG("[VirtualFlowLayout] Visible widgets cleared");
    
    m_cachedFirstVisible = -1;
    m_cachedLastVisible = -1;
    // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
    LOG("[VirtualFlowLayout] Calling calculateLayout()");
    calculateLayout();
    LOG("[VirtualFlowLayout] Calling updateVisibleItems()");
    updateVisibleItems();
    LOG("[VirtualFlowLayout] Calling update()");
    update();
    
    LOG("[VirtualFlowLayout] Restoring scroll position");
    restoreScrollPosition(savedScrollY);
    LOG("[VirtualFlowLayout] refresh() complete");
}

void VirtualFlowLayout::clear()
{
    // Just hide all visible widgets - don't delete them
    // The card manager owns the cards and manages their lifecycle
    for (auto it = m_visibleWidgets.begin(); it != m_visibleWidgets.end(); ++it) {
        if (it.value()) {
            it.value()->hide();
        }
    }
    m_visibleWidgets.clear();
    
    m_itemCount = 0;
    m_cachedFirstVisible = -1;
    m_cachedLastVisible = -1;
    // Let calculateLayout handle its own re-entrancy guard
    calculateLayout();
    update();
}

void VirtualFlowLayout::updateItem(int index)
{
    if (index < 0 || index >= m_itemCount) {
        return;
    }
    
    // If the widget is currently visible, recreate it
    if (m_visibleWidgets.contains(index)) {
        // Recycle the old widget
        recycleWidget(index);
        
        // Create a new widget if still visible
        int firstVisible, lastVisible;
        getVisibleRange(firstVisible, lastVisible);
        
        if (index >= firstVisible && index <= lastVisible) {
            createOrReuseWidget(index);
        }
    }
}

void VirtualFlowLayout::setScrollArea(QScrollArea *scrollArea)
{
    if (m_scrollArea) {
        // Disconnect old scroll area
        if (m_scrollArea->verticalScrollBar()) {
            disconnect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
                      this, &VirtualFlowLayout::onScrollChanged);
        }
        if (m_scrollArea->horizontalScrollBar()) {
            disconnect(m_scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
                      this, &VirtualFlowLayout::onScrollChanged);
        }
        // Remove event filter from old viewport
        if (m_scrollArea->viewport()) {
            m_scrollArea->viewport()->removeEventFilter(this);
        }
    }
    
    m_scrollArea = scrollArea;
    
    if (m_scrollArea) {
        // Connect to scroll events
        if (m_scrollArea->verticalScrollBar()) {
            connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
                   this, &VirtualFlowLayout::onScrollChanged);
        }
        if (m_scrollArea->horizontalScrollBar()) {
            connect(m_scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
                   this, &VirtualFlowLayout::onScrollChanged);
        }
        // Install event filter on viewport to catch resize events
        if (m_scrollArea->viewport()) {
            m_scrollArea->viewport()->installEventFilter(this);
        }
        
        // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
        calculateLayout();
        updateVisibleItems();
    }
}

int VirtualFlowLayout::contentHeight() const
{
    return m_contentHeight;
}

void VirtualFlowLayout::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // Update arrow overlay geometry
    if (m_arrowOverlay) {
        m_arrowOverlay->updateGeometry();
    }
    
    // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
    calculateLayout();
    updateVisibleItems();
}

void VirtualFlowLayout::paintEvent(QPaintEvent *event)
{
    // Paint the background/base widget
    QWidget::paintEvent(event);
    
    // Arrows are now drawn by the overlay widget, so no arrow drawing here
    // Trigger overlay repaint
    if (m_arrowOverlay) {
        m_arrowOverlay->update();
    }
}

void VirtualFlowLayout::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
    calculateLayout();
    updateVisibleItems();
    
    // Update arrow overlay
    if (m_arrowOverlay) {
        m_arrowOverlay->updateGeometry();
        m_arrowOverlay->raise();
    }
}

bool VirtualFlowLayout::event(QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest) {
        // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
        calculateLayout();
        updateVisibleItems();
        return true;
    }
    return QWidget::event(event);
}

bool VirtualFlowLayout::eventFilter(QObject *watched, QEvent *event)
{
    // Handle viewport resize events
    if (m_scrollArea && watched == m_scrollArea->viewport() && event->type() == QEvent::Resize) {
        // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
        calculateLayout();
        updateVisibleItems();
    }
    return QWidget::eventFilter(watched, event);
}

void VirtualFlowLayout::onScrollChanged()
{
    // Let updateVisibleItems handle its own re-entrancy guard
    updateVisibleItems();
}

void VirtualFlowLayout::calculateLayout()
{
    LOG("[VirtualFlowLayout] calculateLayout() started");
    // Static recursion counter to detect infinite recursion (backup check)
    static thread_local int recursionDepth = 0;
    recursionDepth++;
    
    // Re-entrancy guard INSIDE calculateLayout to prevent recursive calls
    // This is the final defense against recursive layout crashes
    if (m_inLayoutUpdate) {
        LOG("[VirtualFlowLayout] Already in layout update, returning");
        recursionDepth--;
        return;
    }
    m_inLayoutUpdate = true;
    auto guardReset = qScopeGuard([this]() { 
        m_inLayoutUpdate = false; 
    });
    
    // Emergency stop if we're recursing too deep
    if (recursionDepth > 10) {
        LOG(QString("[VirtualFlowLayout] Recursion depth %1 exceeded, returning").arg(recursionDepth));
        recursionDepth--;
        return;
    }
    
    // Calculate how many columns fit in the available width
    int availableWidth = width();
    if (m_scrollArea) {
        availableWidth = m_scrollArea->viewport()->width();
    }
    
    if (availableWidth <= 0 || m_itemSize.width() <= 0) {
        m_columnsPerRow = 1;
    } else {
        m_columnsPerRow = qMax(1, (availableWidth + m_hSpacing) / (m_itemSize.width() + m_hSpacing));
    }
    LOG(QString("[VirtualFlowLayout] Columns per row: %1, available width: %2").arg(m_columnsPerRow).arg(availableWidth));
    
    m_rowHeight = m_itemSize.height() + m_vSpacing;
    
    // Calculate total rows needed
    m_totalRows = (m_itemCount + m_columnsPerRow - 1) / m_columnsPerRow;
    
    // Calculate content height
    m_contentHeight = m_totalRows * m_rowHeight;
    if (m_totalRows > 0) {
        m_contentHeight -= m_vSpacing;  // No spacing after last row
    }
    m_contentHeight += 10;  // Small padding at bottom
    LOG(QString("[VirtualFlowLayout] Total rows: %1, content height: %2").arg(m_totalRows).arg(m_contentHeight));
    
    // Update our minimum height to match content
    LOG(QString("[VirtualFlowLayout] Setting minimum height to %1").arg(m_contentHeight));
    setMinimumHeight(m_contentHeight);
    LOG("[VirtualFlowLayout] Minimum height set");
    
    // Collect indices of widgets that may need to be removed (invalid or destroyed)
    QList<int> indicesToRemove;
    
    LOG(QString("[VirtualFlowLayout] Repositioning %1 visible widgets").arg(m_visibleWidgets.size()));
    for (auto it = m_visibleWidgets.begin(); it != m_visibleWidgets.end(); ++it) {
        int index = it.key();
        QWidget *widget = it.value();
        if (widget) {
            // Safety check: verify widget is not in the process of being destroyed
            // A widget without a parent that is also hidden is likely scheduled for deletion
            // via deleteLater() and should not be accessed
            if (!widget->parent() && widget->isHidden()) {
                // Widget is orphaned and hidden - likely scheduled for deletion
                // Skip setGeometry and mark for removal to prevent access violation
                indicesToRemove.append(index);
                continue;
            }
            
            int row = index / m_columnsPerRow;
            int col = index % m_columnsPerRow;
            int x = col * (m_itemSize.width() + m_hSpacing);
            int y = row * m_rowHeight;
            widget->setGeometry(x, y, m_itemSize.width(), m_itemSize.height());
        } else {
            // Null widget pointer in the map - this should not happen
            indicesToRemove.append(index);
        }
    }
    
    // Remove any dead or null widget entries
    if (!indicesToRemove.isEmpty()) {
        LOG(QString("[VirtualFlowLayout] Removing %1 dead widgets").arg(indicesToRemove.size()));
    }
    for (int index : indicesToRemove) {
        m_visibleWidgets.remove(index);
    }
    
    LOG("[VirtualFlowLayout] calculateLayout() complete");
    recursionDepth--;
}

void VirtualFlowLayout::updateVisibleItems()
{
    // Note: We don't need a re-entrancy guard here because:
    // 1. calculateLayout() has a guard that prevents re-entry during setGeometry calls
    // 2. updateVisibleItems is typically called right after calculateLayout from the same entry point
    // 3. If called from a recursive event, calculateLayout's guard will block the chain
    
    LOG("[VirtualFlowLayout] updateVisibleItems() started");
    if (!m_itemFactory || m_itemCount == 0) {
        LOG("[VirtualFlowLayout] No factory or items, returning");
        return;
    }
    
    int firstVisible, lastVisible;
    getVisibleRange(firstVisible, lastVisible);
    LOG(QString("[VirtualFlowLayout] Visible range: %1 to %2").arg(firstVisible).arg(lastVisible));
    
    // Check if the range has changed
    if (firstVisible == m_cachedFirstVisible && lastVisible == m_cachedLastVisible) {
        LOG("[VirtualFlowLayout] Range unchanged, skipping update");
        return;  // No change, skip update
    }
    
    m_cachedFirstVisible = firstVisible;
    m_cachedLastVisible = lastVisible;
    
    // Collect indices that are no longer visible
    QList<int> toRecycle;
    for (auto it = m_visibleWidgets.begin(); it != m_visibleWidgets.end(); ++it) {
        int idx = it.key();
        if (idx < firstVisible || idx > lastVisible) {
            toRecycle.append(idx);
        }
    }
    LOG(QString("[VirtualFlowLayout] Recycling %1 widgets").arg(toRecycle.size()));
    
    // Recycle widgets that are no longer visible
    for (int idx : toRecycle) {
        recycleWidget(idx);
    }
    
    // Create widgets for newly visible items
    int widgetCount = lastVisible - firstVisible + 1;
    LOG(QString("[VirtualFlowLayout] Creating/reusing %1 widgets").arg(widgetCount));
    for (int idx = firstVisible; idx <= lastVisible; ++idx) {
        if (!m_visibleWidgets.contains(idx)) {
            LOG(QString("[VirtualFlowLayout] About to call createOrReuseWidget for index %1").arg(idx));
            createOrReuseWidget(idx);
            LOG(QString("[VirtualFlowLayout] createOrReuseWidget completed for index %1").arg(idx));
        } else {
            LOG(QString("[VirtualFlowLayout] Widget already exists for index %1, skipping").arg(idx));
        }
    }
    LOG("[VirtualFlowLayout] updateVisibleItems() complete");
}

QRect VirtualFlowLayout::visibleRect() const
{
    if (!m_scrollArea) {
        return rect();
    }
    
    QScrollBar *vScrollBar = m_scrollArea->verticalScrollBar();
    QScrollBar *hScrollBar = m_scrollArea->horizontalScrollBar();
    
    int scrollX = hScrollBar ? hScrollBar->value() : 0;
    int scrollY = vScrollBar ? vScrollBar->value() : 0;
    
    QSize viewportSize = m_scrollArea->viewport()->size();
    
    // If viewport size is not yet valid, use a reasonable default
    // This ensures we show at least some cards on initial load
    if (viewportSize.width() <= 0) {
        viewportSize.setWidth(800);  // Default width
    }
    if (viewportSize.height() <= 0) {
        viewportSize.setHeight(600);  // Default height
    }
    
    return QRect(scrollX, scrollY, viewportSize.width(), viewportSize.height());
}

int VirtualFlowLayout::rowAtY(int y) const
{
    if (m_rowHeight <= 0) {
        return 0;
    }
    return y / m_rowHeight;
}

void VirtualFlowLayout::getVisibleRange(int &firstVisible, int &lastVisible) const
{
    if (m_itemCount == 0 || m_columnsPerRow == 0) {
        firstVisible = -1;
        lastVisible = -1;
        return;
    }
    
    QRect visible = visibleRect();
    
    // Calculate first and last visible row (with buffer)
    int firstRow = qMax(0, rowAtY(visible.top()) - BUFFER_ROWS);
    int lastRow = qMin(m_totalRows - 1, rowAtY(visible.bottom()) + BUFFER_ROWS);
    
    // Convert to item indices
    firstVisible = firstRow * m_columnsPerRow;
    lastVisible = qMin(m_itemCount - 1, (lastRow + 1) * m_columnsPerRow - 1);
}

QWidget* VirtualFlowLayout::createOrReuseWidget(int index)
{
    LOG(QString("[VirtualFlowLayout] createOrReuseWidget: entry for index %1").arg(index));
    
    if (index < 0 || index >= m_itemCount || !m_itemFactory) {
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: invalid index or no factory, returning null"));
        return nullptr;
    }
    
    // Check if already in visible widgets map
    if (m_visibleWidgets.contains(index)) {
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: widget already exists for index %1").arg(index));
        return m_visibleWidgets[index];
    }
    
    LOG(QString("[VirtualFlowLayout] createOrReuseWidget: calling m_itemFactory for index %1").arg(index));
    // Get or create widget using factory
    // The factory may return an existing cached card from the card manager
    QWidget *widget = m_itemFactory(index);
    LOG(QString("[VirtualFlowLayout] createOrReuseWidget: m_itemFactory returned widget %1 for index %2").arg(widget ? "non-null" : "null").arg(index));
    
    if (widget) {
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: setting parent for index %1").arg(index));
        // Ensure proper parent (may have been unparented if recycled)
        if (widget->parent() != this) {
            widget->setParent(this);
        }
        
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: positioning widget for index %1").arg(index));
        // Position the widget
        int row = index / m_columnsPerRow;
        int col = index % m_columnsPerRow;
        int x = col * (m_itemSize.width() + m_hSpacing);
        int y = row * m_rowHeight;
        
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: calling setGeometry for index %1").arg(index));
        widget->setGeometry(x, y, m_itemSize.width(), m_itemSize.height());
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: calling show for index %1").arg(index));
        widget->show();
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: show completed for index %1").arg(index));
        
        m_visibleWidgets[index] = widget;
        
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: emitting widgetCreated for index %1").arg(index));
        emit widgetCreated(index, widget);
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: signal emitted for index %1").arg(index));
    }
    
    LOG(QString("[VirtualFlowLayout] createOrReuseWidget: returning for index %1").arg(index));
    return widget;
}

void VirtualFlowLayout::recycleWidget(int index)
{
    if (!m_visibleWidgets.contains(index)) {
        return;
    }
    
    QWidget *widget = m_visibleWidgets.take(index);
    
    if (widget) {
        // Just hide the widget - don't delete it
        // The card manager keeps it in its cache and we'll reuse it when scrolling back
        widget->hide();
    }
}

int VirtualFlowLayout::saveScrollPosition() const
{
    if (m_scrollArea && m_scrollArea->verticalScrollBar()) {
        return m_scrollArea->verticalScrollBar()->value();
    }
    return 0;
}

void VirtualFlowLayout::restoreScrollPosition(int savedY)
{
    if (m_scrollArea && m_scrollArea->verticalScrollBar()) {
        m_scrollArea->verticalScrollBar()->setValue(savedY);
    }
}
