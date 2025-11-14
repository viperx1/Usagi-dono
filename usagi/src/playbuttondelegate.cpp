#include "playbuttondelegate.h"
#include <QPainter>
#include <QApplication>
#include <QMouseEvent>
#include <QStyleOptionButton>

PlayButtonDelegate::PlayButtonDelegate(QObject *parent)
    : QStyledItemDelegate(parent), m_isPressed(false)
{
}

void PlayButtonDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    if (!index.isValid())
        return;

    QString text = index.data(Qt::DisplayRole).toString();
    
    // If there's no play button text (empty), just paint default
    if (text.isEmpty()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // Draw button background with less aggressive margins to make button more visible
    QStyleOptionButton buttonOption;
    buttonOption.rect = option.rect.adjusted(1, 1, -1, -1);  // Reduced from (2,2,-2,-2) to (1,1,-1,-1)
    buttonOption.state = QStyle::State_Enabled;
    
    // Add hover effect
    if (m_hoveredIndex == index) {
        buttonOption.state |= QStyle::State_MouseOver;
    }
    
    // Add pressed effect
    if (m_isPressed && m_hoveredIndex == index) {
        buttonOption.state |= QStyle::State_Sunken;
    } else {
        buttonOption.state |= QStyle::State_Raised;
    }
    
    // Draw the button
    QApplication::style()->drawControl(QStyle::CE_PushButton, &buttonOption, painter);
    
    // Draw the text (play icon or checkmark) centered
    painter->save();
    QFont font = painter->font();
    font.setPointSize(font.pointSize() + 2); // Make icon slightly larger
    painter->setFont(font);
    painter->drawText(buttonOption.rect, Qt::AlignCenter, text);
    painter->restore();
}

bool PlayButtonDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                     const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (!index.isValid())
        return false;

    QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty())
        return false;

    if (event->type() == QEvent::MouseMove) {
        m_hoveredIndex = index;
        // Trigger repaint for hover effect
        if (QTreeWidget *tree = qobject_cast<QTreeWidget*>(const_cast<QAbstractItemModel*>(model)->parent())) {
            tree->viewport()->update(option.rect);
        }
        return true;
    }
    else if (event->type() == QEvent::Leave) {
        m_hoveredIndex = QModelIndex();
        if (QTreeWidget *tree = qobject_cast<QTreeWidget*>(const_cast<QAbstractItemModel*>(model)->parent())) {
            tree->viewport()->update();
        }
        return true;
    }
    else if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            m_isPressed = true;
            m_hoveredIndex = index;
            if (QTreeWidget *tree = qobject_cast<QTreeWidget*>(const_cast<QAbstractItemModel*>(model)->parent())) {
                tree->viewport()->update(option.rect);
            }
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_isPressed) {
            m_isPressed = false;
            if (m_hoveredIndex == index) {
                emit playButtonClicked(index);
            }
            if (QTreeWidget *tree = qobject_cast<QTreeWidget*>(const_cast<QAbstractItemModel*>(model)->parent())) {
                tree->viewport()->update(option.rect);
            }
            return true;
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QSize PlayButtonDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    // Make button wider and taller for better visibility
    size.setWidth(qMax(48, size.width()));  // Increased from 40 to 48
    size.setHeight(qMax(24, size.height())); // Ensure minimum height of 24 pixels
    return size;
}
