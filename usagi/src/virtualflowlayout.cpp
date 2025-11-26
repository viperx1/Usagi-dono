#include "virtualflowlayout.h"
#include "logger.h"
#include <QScrollBar>
#include <QResizeEvent>
#include <QPainter>
#include <QApplication>
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
{
    setMinimumSize(m_itemSize);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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
    calculateLayout();
    updateVisibleItems();
    update();
}

void VirtualFlowLayout::setItemSize(const QSize &size)
{
    if (m_itemSize == size) {
        return;
    }
    
    m_itemSize = size;
    m_rowHeight = size.height() + m_vSpacing;
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
    // Force recalculation and update of all visible items
    m_cachedFirstVisible = -1;
    m_cachedLastVisible = -1;
    calculateLayout();
    updateVisibleItems();
    update();
}

void VirtualFlowLayout::clear()
{
    // Delete all visible widgets
    for (auto it = m_visibleWidgets.begin(); it != m_visibleWidgets.end(); ++it) {
        if (it.value()) {
            it.value()->setParent(nullptr);
            delete it.value();
        }
    }
    m_visibleWidgets.clear();
    
    // Delete all recycled widgets
    qDeleteAll(m_recycledWidgets);
    m_recycledWidgets.clear();
    
    m_itemCount = 0;
    m_cachedFirstVisible = -1;
    m_cachedLastVisible = -1;
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
    calculateLayout();
    updateVisibleItems();
}

void VirtualFlowLayout::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    // No custom painting needed - widgets handle their own painting
}

bool VirtualFlowLayout::event(QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest) {
        calculateLayout();
        updateVisibleItems();
        return true;
    }
    return QWidget::event(event);
}

void VirtualFlowLayout::onScrollChanged()
{
    updateVisibleItems();
}

void VirtualFlowLayout::calculateLayout()
{
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
    
    m_rowHeight = m_itemSize.height() + m_vSpacing;
    
    // Calculate total rows needed
    m_totalRows = (m_itemCount + m_columnsPerRow - 1) / m_columnsPerRow;
    
    // Calculate content height
    m_contentHeight = m_totalRows * m_rowHeight;
    if (m_totalRows > 0) {
        m_contentHeight -= m_vSpacing;  // No spacing after last row
    }
    m_contentHeight += 10;  // Small padding at bottom
    
    // Update our minimum height to match content
    setMinimumHeight(m_contentHeight);
    
    // Reposition visible widgets
    for (auto it = m_visibleWidgets.begin(); it != m_visibleWidgets.end(); ++it) {
        int index = it.key();
        QWidget *widget = it.value();
        if (widget) {
            int row = index / m_columnsPerRow;
            int col = index % m_columnsPerRow;
            int x = col * (m_itemSize.width() + m_hSpacing);
            int y = row * m_rowHeight;
            widget->setGeometry(x, y, m_itemSize.width(), m_itemSize.height());
        }
    }
}

void VirtualFlowLayout::updateVisibleItems()
{
    if (!m_itemFactory || m_itemCount == 0) {
        return;
    }
    
    int firstVisible, lastVisible;
    getVisibleRange(firstVisible, lastVisible);
    
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
    if (index < 0 || index >= m_itemCount || !m_itemFactory) {
        return nullptr;
    }
    
    // Check if already created
    if (m_visibleWidgets.contains(index)) {
        return m_visibleWidgets[index];
    }
    
    QWidget *widget = nullptr;
    
    // Try to reuse a recycled widget
    // Note: For AnimeCards, we can't really reuse them as they contain specific data
    // So we just create new ones. The factory should handle any recycling logic.
    
    // Create new widget using factory
    widget = m_itemFactory(index);
    
    if (widget) {
        widget->setParent(this);
        
        // Position the widget
        int row = index / m_columnsPerRow;
        int col = index % m_columnsPerRow;
        int x = col * (m_itemSize.width() + m_hSpacing);
        int y = row * m_rowHeight;
        
        widget->setGeometry(x, y, m_itemSize.width(), m_itemSize.height());
        widget->show();
        
        m_visibleWidgets[index] = widget;
        
        emit widgetCreated(index, widget);
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
        widget->hide();
        widget->setParent(nullptr);
        
        // For now, just delete the widget
        // In the future, could implement a recycle pool for compatible widgets
        delete widget;
    }
}
