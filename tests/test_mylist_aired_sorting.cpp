#include <QtTest/QtTest>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include "../usagi/src/aired.h"

// Need to declare the AnimeTreeWidgetItem class for testing
// This is defined in window.h but we don't want to include the whole window
class AnimeTreeWidgetItem : public QTreeWidgetItem
{
public:
    AnimeTreeWidgetItem(QTreeWidget *parent) : QTreeWidgetItem(parent) {}
    
    void setAired(const aired& airedDates) { m_aired = airedDates; }
    aired getAired() const { return m_aired; }
    
    bool operator<(const QTreeWidgetItem &other) const override
    {
        int column = treeWidget()->sortColumn();
        
        // If sorting by Aired column (column 9) and both items have aired data
        if(column == 9)
        {
            const AnimeTreeWidgetItem *otherAnime = dynamic_cast<const AnimeTreeWidgetItem*>(&other);
            if(otherAnime && m_aired.isValid() && otherAnime->m_aired.isValid())
            {
                return m_aired < otherAnime->m_aired;
            }
            // If one has aired data and the other doesn't, put the one with data first
            else if(m_aired.isValid() && otherAnime && !otherAnime->m_aired.isValid())
            {
                return true; // this item comes before other
            }
            else if(!m_aired.isValid() && otherAnime && otherAnime->m_aired.isValid())
            {
                return false; // other item comes before this
            }
        }
        
        // Default comparison for other columns
        return QTreeWidgetItem::operator<(other);
    }
    
private:
    aired m_aired;
};

/**
 * Test to verify that mylist sorts by aired dates correctly
 * 
 * This test verifies that:
 * - Anime items sort by actual air dates, not by string comparison
 * - Both start date and end date are considered (start date is primary)
 * - Invalid dates are handled correctly
 */
class TestMylistAiredSorting : public QObject
{
    Q_OBJECT

private slots:
    void testSortByStartDateAscending();
    void testSortByStartDateDescending();
    void testSortWithInvalidDates();
    void testSameStartDateDifferentEndDate();
    void testStringVsDateComparison();
};

void TestMylistAiredSorting::testSortByStartDateAscending()
{
    // Create a tree widget with aired column (column 9)
    QTreeWidget treeWidget;
    treeWidget.setColumnCount(10);
    treeWidget.setSortingEnabled(false); // Disable auto-sorting while adding items
    
    // Add anime with different start dates
    // Order: 2021, 1998, 2020, 2000
    AnimeTreeWidgetItem *anime1 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired1("2021-04-01", "2021-09-30");
    anime1->setText(0, "Anime 2021");
    anime1->setText(9, aired1.toDisplayString());
    anime1->setAired(aired1);
    
    AnimeTreeWidgetItem *anime2 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired2("1998-04-03", "1999-04-23");
    anime2->setText(0, "Anime 1998");
    anime2->setText(9, aired2.toDisplayString());
    anime2->setAired(aired2);
    
    AnimeTreeWidgetItem *anime3 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired3("2020-01-15", "2020-12-31");
    anime3->setText(0, "Anime 2020");
    anime3->setText(9, aired3.toDisplayString());
    anime3->setAired(aired3);
    
    AnimeTreeWidgetItem *anime4 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired4("2000-01-01", "2000-12-31");
    anime4->setText(0, "Anime 2000");
    anime4->setText(9, aired4.toDisplayString());
    anime4->setAired(aired4);
    
    treeWidget.addTopLevelItem(anime1);
    treeWidget.addTopLevelItem(anime2);
    treeWidget.addTopLevelItem(anime3);
    treeWidget.addTopLevelItem(anime4);
    
    // Sort by aired column (column 9) in ascending order
    treeWidget.sortItems(9, Qt::AscendingOrder);
    
    // Verify the order: 1998, 2000, 2020, 2021
    QCOMPARE(treeWidget.topLevelItem(0)->text(0), QString("Anime 1998"));
    QCOMPARE(treeWidget.topLevelItem(1)->text(0), QString("Anime 2000"));
    QCOMPARE(treeWidget.topLevelItem(2)->text(0), QString("Anime 2020"));
    QCOMPARE(treeWidget.topLevelItem(3)->text(0), QString("Anime 2021"));
}

void TestMylistAiredSorting::testSortByStartDateDescending()
{
    // Create a tree widget with aired column (column 9)
    QTreeWidget treeWidget;
    treeWidget.setColumnCount(10);
    treeWidget.setSortingEnabled(false);
    
    // Add anime with different start dates
    AnimeTreeWidgetItem *anime1 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired1("2021-04-01", "2021-09-30");
    anime1->setText(0, "Anime 2021");
    anime1->setText(9, aired1.toDisplayString());
    anime1->setAired(aired1);
    
    AnimeTreeWidgetItem *anime2 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired2("1998-04-03", "1999-04-23");
    anime2->setText(0, "Anime 1998");
    anime2->setText(9, aired2.toDisplayString());
    anime2->setAired(aired2);
    
    AnimeTreeWidgetItem *anime3 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired3("2020-01-15", "2020-12-31");
    anime3->setText(0, "Anime 2020");
    anime3->setText(9, aired3.toDisplayString());
    anime3->setAired(aired3);
    
    treeWidget.addTopLevelItem(anime1);
    treeWidget.addTopLevelItem(anime2);
    treeWidget.addTopLevelItem(anime3);
    
    // Sort by aired column (column 9) in descending order
    treeWidget.sortItems(9, Qt::DescendingOrder);
    
    // Verify the order: 2021, 2020, 1998
    QCOMPARE(treeWidget.topLevelItem(0)->text(0), QString("Anime 2021"));
    QCOMPARE(treeWidget.topLevelItem(1)->text(0), QString("Anime 2020"));
    QCOMPARE(treeWidget.topLevelItem(2)->text(0), QString("Anime 1998"));
}

void TestMylistAiredSorting::testSortWithInvalidDates()
{
    // Create a tree widget
    QTreeWidget treeWidget;
    treeWidget.setColumnCount(10);
    treeWidget.setSortingEnabled(false);
    
    // Add anime with valid dates
    AnimeTreeWidgetItem *anime1 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired1("2020-01-01", "2020-12-31");
    anime1->setText(0, "Anime 2020");
    anime1->setText(9, aired1.toDisplayString());
    anime1->setAired(aired1);
    
    // Add anime with invalid/empty dates
    AnimeTreeWidgetItem *anime2 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired2("", "");
    anime2->setText(0, "Anime No Date");
    anime2->setText(9, aired2.toDisplayString());
    anime2->setAired(aired2);
    
    // Add another anime with valid dates
    AnimeTreeWidgetItem *anime3 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired3("2010-01-01", "2010-12-31");
    anime3->setText(0, "Anime 2010");
    anime3->setText(9, aired3.toDisplayString());
    anime3->setAired(aired3);
    
    treeWidget.addTopLevelItem(anime1);
    treeWidget.addTopLevelItem(anime2);
    treeWidget.addTopLevelItem(anime3);
    
    // Sort by aired column (column 9) in ascending order
    treeWidget.sortItems(9, Qt::AscendingOrder);
    
    // Verify that valid dates come before invalid ones
    // Order should be: 2010, 2020, No Date
    QCOMPARE(treeWidget.topLevelItem(0)->text(0), QString("Anime 2010"));
    QCOMPARE(treeWidget.topLevelItem(1)->text(0), QString("Anime 2020"));
    QCOMPARE(treeWidget.topLevelItem(2)->text(0), QString("Anime No Date"));
}

void TestMylistAiredSorting::testSameStartDateDifferentEndDate()
{
    // Test that when start dates are the same, end dates are used for sorting
    QTreeWidget treeWidget;
    treeWidget.setColumnCount(10);
    treeWidget.setSortingEnabled(false);
    
    // All anime start on 2020-01-01 but end on different dates
    AnimeTreeWidgetItem *anime1 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired1("2020-01-01", "2020-12-31");
    anime1->setText(0, "Anime End Dec");
    anime1->setText(9, aired1.toDisplayString());
    anime1->setAired(aired1);
    
    AnimeTreeWidgetItem *anime2 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired2("2020-01-01", "2020-03-31");
    anime2->setText(0, "Anime End Mar");
    anime2->setText(9, aired2.toDisplayString());
    anime2->setAired(aired2);
    
    AnimeTreeWidgetItem *anime3 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired3("2020-01-01", "2020-06-30");
    anime3->setText(0, "Anime End Jun");
    anime3->setText(9, aired3.toDisplayString());
    anime3->setAired(aired3);
    
    treeWidget.addTopLevelItem(anime1);
    treeWidget.addTopLevelItem(anime2);
    treeWidget.addTopLevelItem(anime3);
    
    // Sort by aired column in ascending order
    treeWidget.sortItems(9, Qt::AscendingOrder);
    
    // Verify order: End Mar, End Jun, End Dec (sorted by end date)
    QCOMPARE(treeWidget.topLevelItem(0)->text(0), QString("Anime End Mar"));
    QCOMPARE(treeWidget.topLevelItem(1)->text(0), QString("Anime End Jun"));
    QCOMPARE(treeWidget.topLevelItem(2)->text(0), QString("Anime End Dec"));
}

void TestMylistAiredSorting::testStringVsDateComparison()
{
    // This is the key test - verify that dates sort correctly and not as strings
    // String sorting would put "03.11.2020" before "28.01.2020" (alphabetically)
    // But date sorting should put 28.01.2020 before 03.11.2020 (chronologically)
    
    QTreeWidget treeWidget;
    treeWidget.setColumnCount(10);
    treeWidget.setSortingEnabled(false);
    
    // Dates that would sort incorrectly as strings
    AnimeTreeWidgetItem *anime1 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired1("2020-11-03", "2020-11-03"); // Display: "03.11.2020-03.11.2020"
    anime1->setText(0, "November Anime");
    anime1->setText(9, aired1.toDisplayString());
    anime1->setAired(aired1);
    
    AnimeTreeWidgetItem *anime2 = new AnimeTreeWidgetItem(&treeWidget);
    aired aired2("2020-01-28", "2020-01-28"); // Display: "28.01.2020-28.01.2020"
    anime2->setText(0, "January Anime");
    anime2->setText(9, aired2.toDisplayString());
    anime2->setAired(aired2);
    
    treeWidget.addTopLevelItem(anime1);
    treeWidget.addTopLevelItem(anime2);
    
    // Sort by aired column in ascending order
    treeWidget.sortItems(9, Qt::AscendingOrder);
    
    // If sorting by date: January (2020-01-28) should come before November (2020-11-03)
    // If sorting by string: "03.11.2020" would come before "28.01.2020" (WRONG!)
    QCOMPARE(treeWidget.topLevelItem(0)->text(0), QString("January Anime"));
    QCOMPARE(treeWidget.topLevelItem(1)->text(0), QString("November Anime"));
}

QTEST_MAIN(TestMylistAiredSorting)
#include "test_mylist_aired_sorting.moc"
