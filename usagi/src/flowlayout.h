#ifndef FLOWLAYOUT_H
#define FLOWLAYOUT_H

#include <QLayout>
#include <QRect>
#include <QStyle>

/**
 * FlowLayout - A layout that arranges items horizontally, wrapping to next row when needed
 * 
 * This layout is used to display anime cards in a grid-like fashion where cards
 * are arranged left-to-right, and wrap to the next line when the row is full.
 */
class FlowLayout : public QLayout
{
    Q_OBJECT
    
public:
    explicit FlowLayout(QWidget *parent, int margin = -1, int hSpacing = -1, int vSpacing = -1);
    explicit FlowLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1);
    virtual ~FlowLayout();

    void addItem(QLayoutItem *item) override;
    int horizontalSpacing() const;
    int verticalSpacing() const;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;
    QLayoutItem *takeAt(int index) override;

private:
    int doLayout(const QRect &rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem *> m_itemList;
    int m_hSpace;
    int m_vSpace;
};

#endif // FLOWLAYOUT_H
