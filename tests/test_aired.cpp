#include <QtTest/QtTest>
#include "../usagi/src/aired.h"
#include <QDate>

class TestAired : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstructor();
    void testFinishedAnime();
    void testOngoingAnime();
    void testFutureAnime();
    void testEmptyDates();
    void testOnlyStartDate();
    void testComparison();
    void testDateParsing();
};

void TestAired::testDefaultConstructor()
{
    aired a;
    QVERIFY(!a.isValid());
    QVERIFY(!a.hasStartDate());
    QVERIFY(!a.hasEndDate());
    QCOMPARE(a.toDisplayString(), QString(""));
}

void TestAired::testFinishedAnime()
{
    // Test with dates in the past (finished anime)
    aired a("1988-07-21Z", "1989-04-21Z");
    QVERIFY(a.isValid());
    QVERIFY(a.hasStartDate());
    QVERIFY(a.hasEndDate());
    QCOMPARE(a.toDisplayString(), QString("21.07.1988-21.04.1989"));
}

void TestAired::testOngoingAnime()
{
    // Test with start date in past and end date in future (ongoing)
    QDate today = QDate::currentDate();
    QDate pastDate = today.addDays(-365);
    QDate futureDate = today.addDays(365);
    
    aired a(pastDate, futureDate);
    QVERIFY(a.isValid());
    QString result = a.toDisplayString();
    QVERIFY(result.contains("-ongoing"));
    QVERIFY(result.startsWith(pastDate.toString("dd.MM.yyyy")));
}

void TestAired::testFutureAnime()
{
    // Test with start date in future
    QDate futureDate = QDate::currentDate().addDays(365);
    
    aired a(futureDate, QDate());
    QVERIFY(a.isValid());
    QString result = a.toDisplayString();
    QVERIFY(result.startsWith("Airs "));
    QVERIFY(result.contains(futureDate.toString("dd.MM.yyyy")));
}

void TestAired::testEmptyDates()
{
    aired a("", "");
    QVERIFY(!a.isValid());
    QCOMPARE(a.toDisplayString(), QString(""));
}

void TestAired::testOnlyStartDate()
{
    // Test with only start date (should show as ongoing)
    aired a("2020-01-15Z", "");
    QVERIFY(a.isValid());
    QVERIFY(a.hasStartDate());
    QVERIFY(!a.hasEndDate());
    QString result = a.toDisplayString();
    QVERIFY(result.contains("-ongoing"));
    QVERIFY(result.startsWith("15.01.2020"));
}

void TestAired::testComparison()
{
    aired a1("2020-01-15Z", "2020-12-31Z");
    aired a2("2021-04-01Z", "2021-09-30Z");
    aired a3("2020-01-15Z", "2020-12-31Z");
    
    QVERIFY(a1 < a2);
    QVERIFY(a2 > a1);
    QVERIFY(a1 == a3);
    QVERIFY(a1 != a2);
}

void TestAired::testDateParsing()
{
    // Test parsing with 'Z' suffix
    aired a1("2003-11-16Z", "2003-11-16Z");
    QVERIFY(a1.isValid());
    QCOMPARE(a1.startDate(), QDate(2003, 11, 16));
    QCOMPARE(a1.endDate(), QDate(2003, 11, 16));
    
    // Test parsing without 'Z' suffix
    aired a2("2002-06-20", "2003-04-10");
    QVERIFY(a2.isValid());
    QCOMPARE(a2.startDate(), QDate(2002, 6, 20));
    QCOMPARE(a2.endDate(), QDate(2003, 4, 10));
}

QTEST_MAIN(TestAired)
#include "test_aired.moc"
