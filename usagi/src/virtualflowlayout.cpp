#include "virtualflowlayout.h"
#include "logger.h"
#include <QScrollBar>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPainter>
#include <QApplication>
#include <QScopeGuard>
#include <algorithm>

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
    , m_deferredUpdateTimer(nullptr)
    , m_inLayoutUpdate(false)
{
    setMinimumSize(m_itemSize);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Create deferred update timer for handling initialization timing issues
    m_deferredUpdateTimer = new QTimer(this);
    m_deferredUpdateTimer->setSingleShot(true);
    m_deferredUpdateTimer->setInterval(100);  // 100ms delay
    connect(m_deferredUpdateTimer, &QTimer::timeout, this, [this]() {
        LOG("[VirtualFlowLayout] Deferred update triggered");
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
    LOG(QString("[VirtualFlowLayout] setItemCount: changing from %1 to %2").arg(m_itemCount).arg(count));
    
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

void VirtualFlowLayout::setItemSize(const QSize &size)
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
    int savedScrollY = saveScrollPosition();
    
    // Force recalculation and update of all visible items
    // We need to hide and clear all visible widgets first because after
    // sorting/filtering, the same index may now represent a different item
    for (auto it = m_visibleWidgets.begin(); it != m_visibleWidgets.end(); ++it) {
        if (it.value()) {
            it.value()->hide();
        }
    }
    m_visibleWidgets.clear();
    
    m_cachedFirstVisible = -1;
    m_cachedLastVisible = -1;
    // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
    calculateLayout();
    updateVisibleItems();
    update();
    
    restoreScrollPosition(savedScrollY);
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
    LOG(QString("[VirtualFlowLayout] resizeEvent: guard=%1").arg(m_inLayoutUpdate ? "SET" : "NOT SET"));
    // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
    calculateLayout();
    updateVisibleItems();
}

void VirtualFlowLayout::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    // No custom painting needed - widgets handle their own painting
}

void VirtualFlowLayout::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    LOG(QString("[VirtualFlowLayout] showEvent triggered, guard=%1").arg(m_inLayoutUpdate ? "SET" : "NOT SET"));
    // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
    calculateLayout();
    updateVisibleItems();
}

bool VirtualFlowLayout::event(QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest) {
        LOG(QString("[VirtualFlowLayout] LayoutRequest event, guard=%1").arg(m_inLayoutUpdate ? "SET" : "NOT SET"));
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
        LOG(QString("[VirtualFlowLayout] eventFilter: Viewport resized, guard_before=%1").arg(m_inLayoutUpdate ? "SET" : "NOT SET"));
        // Let calculateLayout and updateVisibleItems handle their own re-entrancy guards
        calculateLayout();
        updateVisibleItems();
        LOG(QString("[VirtualFlowLayout] eventFilter: Viewport resize done, guard_after=%1").arg(m_inLayoutUpdate ? "SET" : "NOT SET"));
    }
    return QWidget::eventFilter(watched, event);
}

void VirtualFlowLayout::onScrollChanged()
{
    LOG(QString("[VirtualFlowLayout] onScrollChanged: guard=%1").arg(m_inLayoutUpdate ? "SET" : "NOT SET"));
    // Let updateVisibleItems handle its own re-entrancy guard
    
    updateVisibleItems();
}

void VirtualFlowLayout::calculateLayout()
{
    // Static recursion counter to detect infinite recursion (backup check)
    static thread_local int recursionDepth = 0;
    recursionDepth++;
    
    // Debug: Log guard state BEFORE any changes
    LOG(QString("[VirtualFlowLayout] calculateLayout ENTER: guard_before=%1, this=%2, depth=%3")
        .arg(m_inLayoutUpdate ? "SET" : "NOT SET").arg(reinterpret_cast<quintptr>(this)).arg(recursionDepth));
    
    // Re-entrancy guard INSIDE calculateLayout to prevent recursive calls
    // This is the final defense against recursive layout crashes
    if (m_inLayoutUpdate) {
        LOG(QString("[VirtualFlowLayout] calculateLayout BLOCKED: guard already SET, this=%1, depth=%2")
            .arg(reinterpret_cast<quintptr>(this)).arg(recursionDepth));
        recursionDepth--;
        return;
    }
    m_inLayoutUpdate = true;
    auto guardReset = qScopeGuard([this]() { 
        LOG(QString("[VirtualFlowLayout] calculateLayout: resetting guard inside calculateLayout, this=%1")
            .arg(reinterpret_cast<quintptr>(this)));
        m_inLayoutUpdate = false; 
    });
    
    // Emergency stop if we're recursing too deep
    if (recursionDepth > 10) {
        LOG(QString("[VirtualFlowLayout] calculateLayout: RECURSION DETECTED! depth=%1, ABORTING").arg(recursionDepth));
        recursionDepth--;
        return;
    }
    
    // Calculate how many columns fit in the available width
    int availableWidth = width();
    if (m_scrollArea) {
        availableWidth = m_scrollArea->viewport()->width();
    }
    
    LOG(QString("[VirtualFlowLayout] calculateLayout: availableWidth=%1, itemSize=(%2,%3), itemCount=%4")
        .arg(availableWidth).arg(m_itemSize.width()).arg(m_itemSize.height()).arg(m_itemCount));
    
    if (availableWidth <= 0 || m_itemSize.width() <= 0) {
        m_columnsPerRow = 1;
    } else {
        m_columnsPerRow = qMax(1, (availableWidth + m_hSpacing) / (m_itemSize.width() + m_hSpacing));
    }
    
    m_rowHeight = m_itemSize.height() + m_vSpacing;
    
    // Calculate total rows needed
    m_totalRows = (m_itemCount + m_columnsPerRow - 1) / m_columnsPerRow;
    
    // Calculate content height
    m_contentHeight = m_totalRows * m_rowHeight;
    if (m_totalRows > 0) {
        m_contentHeight -= m_vSpacing;  // No spacing after last row
    }
    m_contentHeight += 10;  // Small padding at bottom
    
    LOG(QString("[VirtualFlowLayout] calculateLayout: columnsPerRow=%1, totalRows=%2, contentHeight=%3")
        .arg(m_columnsPerRow).arg(m_totalRows).arg(m_contentHeight));
    
    // Update our minimum height to match content
    LOG("[VirtualFlowLayout] calculateLayout: about to call setMinimumHeight");
    setMinimumHeight(m_contentHeight);
    LOG("[VirtualFlowLayout] calculateLayout: setMinimumHeight done");
    
    // Reposition visible widgets
    int widgetCount = m_visibleWidgets.size();
    LOG(QString("[VirtualFlowLayout] calculateLayout: repositioning %1 visible widgets").arg(widgetCount));
    
    for (auto it = m_visibleWidgets.begin(); it != m_visibleWidgets.end(); ++it) {
        int index = it.key();
        QWidget *widget = it.value();
        if (widget) {
            int row = index / m_columnsPerRow;
            int col = index % m_columnsPerRow;
            int x = col * (m_itemSize.width() + m_hSpacing);
            int y = row * m_rowHeight;
            LOG(QString("[VirtualFlowLayout] calculateLayout: setGeometry for widget %1 at (%2,%3) size (%4,%5)")
                .arg(index).arg(x).arg(y).arg(m_itemSize.width()).arg(m_itemSize.height()));
            widget->setGeometry(x, y, m_itemSize.width(), m_itemSize.height());
            LOG(QString("[VirtualFlowLayout] calculateLayout: setGeometry done for widget %1").arg(index));
        }
    }
    
    LOG(QString("[VirtualFlowLayout] calculateLayout EXIT: guard=%1, depth=%2").arg(m_inLayoutUpdate ? "SET" : "NOT SET").arg(recursionDepth));
    recursionDepth--;
}

void VirtualFlowLayout::updateVisibleItems()
{
    // Note: We don't need a re-entrancy guard here because:
    // 1. calculateLayout() has a guard that prevents re-entry during setGeometry calls
    // 2. updateVisibleItems is typically called right after calculateLayout from the same entry point
    // 3. If called from a recursive event, calculateLayout's guard will block the chain
    
    LOG(QString("[VirtualFlowLayout] updateVisibleItems ENTER: guard=%1").arg(m_inLayoutUpdate ? "SET" : "NOT SET"));
    
    if (!m_itemFactory || m_itemCount == 0) {
        LOG(QString("[VirtualFlowLayout] updateVisibleItems skipped: factory=%1, itemCount=%2")
            .arg(m_itemFactory ? "yes" : "no").arg(m_itemCount));
        return;
    }
    
    int firstVisible, lastVisible;
    getVisibleRange(firstVisible, lastVisible);
    
    LOG(QString("[VirtualFlowLayout] updateVisibleItems: range=%1-%2, cachedRange=%3-%4")
        .arg(firstVisible).arg(lastVisible).arg(m_cachedFirstVisible).arg(m_cachedLastVisible));
    
    // Check if the range has changed
    if (firstVisible == m_cachedFirstVisible && lastVisible == m_cachedLastVisible) {
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
    
    // Recycle widgets that are no longer visible
    for (int idx : toRecycle) {
        recycleWidget(idx);
    }
    
    // Create widgets for newly visible items
    for (int idx = firstVisible; idx <= lastVisible; ++idx) {
        if (!m_visibleWidgets.contains(idx)) {
            createOrReuseWidget(idx);
        }
    }
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
    
    LOG(QString("[VirtualFlowLayout] getVisibleRange: visibleRect=(%1,%2,%3,%4), rowHeight=%5, totalRows=%6, columnsPerRow=%7")
        .arg(visible.x()).arg(visible.y()).arg(visible.width()).arg(visible.height())
        .arg(m_rowHeight).arg(m_totalRows).arg(m_columnsPerRow));
    
    // Calculate first and last visible row (with buffer)
    int firstRow = qMax(0, rowAtY(visible.top()) - BUFFER_ROWS);
    int lastRow = qMin(m_totalRows - 1, rowAtY(visible.bottom()) + BUFFER_ROWS);
    
    // Convert to item indices
    firstVisible = firstRow * m_columnsPerRow;
    lastVisible = qMin(m_itemCount - 1, (lastRow + 1) * m_columnsPerRow - 1);
}

QWidget* VirtualFlowLayout::createOrReuseWidget(int index)
{
    if (index < 0 || index >= m_itemCount || !m_itemFactory) {
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: skipped index=%1 (count=%2, factory=%3)")
            .arg(index).arg(m_itemCount).arg(m_itemFactory ? "yes" : "no"));
        return nullptr;
    }
    
    // Check if already in visible widgets map
    if (m_visibleWidgets.contains(index)) {
        return m_visibleWidgets[index];
    }
    
    LOG(QString("[VirtualFlowLayout] createOrReuseWidget: creating/reusing widget for index=%1, guard=%2")
        .arg(index).arg(m_inLayoutUpdate ? "SET" : "NOT SET"));
    
    // Get or create widget using factory
    // The factory may return an existing cached card from the card manager
    QWidget *widget = m_itemFactory(index);
    
    if (widget) {
        // Ensure proper parent (may have been unparented if recycled)
        if (widget->parent() != this) {
            widget->setParent(this);
        }
        
        // Position the widget
        int row = index / m_columnsPerRow;
        int col = index % m_columnsPerRow;
        int x = col * (m_itemSize.width() + m_hSpacing);
        int y = row * m_rowHeight;
        
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: about to setGeometry for widget %1 at (%2,%3), guard=%4")
            .arg(index).arg(x).arg(y).arg(m_inLayoutUpdate ? "SET" : "NOT SET"));
        widget->setGeometry(x, y, m_itemSize.width(), m_itemSize.height());
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: setGeometry done for widget %1").arg(index));
        widget->show();
        
        m_visibleWidgets[index] = widget;
        
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: widget at (%1,%2) size (%3,%4)")
            .arg(x).arg(y).arg(m_itemSize.width()).arg(m_itemSize.height()));
        
        emit widgetCreated(index, widget);
    } else {
        LOG(QString("[VirtualFlowLayout] createOrReuseWidget: factory returned null for index=%1").arg(index));
    }
    
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
