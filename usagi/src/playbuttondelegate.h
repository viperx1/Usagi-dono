#ifndef PLAYBUTTONDELEGATE_H
#define PLAYBUTTONDELEGATE_H

#include <QStyledItemDelegate>
#include <QPushButton>
#include <QTreeWidget>

class PlayButtonDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit PlayButtonDelegate(QObject *parent = nullptr);
    
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                    const QStyleOptionViewItem &option, const QModelIndex &index) override;
    
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

signals:
    void playButtonClicked(const QModelIndex &index);

private:
    mutable QModelIndex m_hoveredIndex;
    mutable bool m_isPressed;
};

#endif // PLAYBUTTONDELEGATE_H
