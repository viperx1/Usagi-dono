#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QDir>
#include <QFile>

/**
 * Comprehensive test suite for the MyList filter system
 * 
 * Tests all filter types:
 * 1. Search filter (text search in titles)
 * 2. Type filter (TV, Movie, OVA, etc.)
 * 3. Completion filter (Completed, Watching, Not Started)
 * 4. Unwatched episodes filter
 * 5. In MyList filter
 * 6. Adult content filter (18+)
 * 7. Series chain display (tested separately in test_animechain.cpp)
 * 
 * Tests filter combinations and edge cases
 */
class TestFilterSystem : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Individual filter tests
    void testSearchFilter_exactMatch();
    void testSearchFilter_partialMatch();
    void testSearchFilter_caseInsensitive();
    void testSearchFilter_alternativeTitles();
    void testSearchFilter_emptySearch();
    void testSearchFilter_noMatch();
    
    void testTypeFilter_tvSeries();
    void testTypeFilter_movie();
    void testTypeFilter_ova();
    void testTypeFilter_noFilter();
    
    void testCompletionFilter_completed();
    void testCompletionFilter_watching();
    void testCompletionFilter_notStarted();
    void testCompletionFilter_unknownTotal();
    void testCompletionFilter_noEpisodes();
    
    void testUnwatchedFilter_hasUnwatchedNormal();
    void testUnwatchedFilter_hasUnwatchedOther();
    void testUnwatchedFilter_allWatched();
    void testUnwatchedFilter_noneWatched();
    
    void testAdultContentFilter_hide();
    void testAdultContentFilter_showOnly();
    void testAdultContentFilter_ignore();
    
    void testInMyListFilter_onlyMyList();
    void testInMyListFilter_allAnime();
    
    // Filter combination tests
    void testCombination_typeAndCompletion();
    void testCombination_searchAndType();
    void testCombination_allFiltersActive();
    void testCombination_multipleAnime();
    
    // Edge case tests
    void testEdgeCase_emptyDatabase();
    void testEdgeCase_missingData();
    void testEdgeCase_zeroEpisodes();
    
    // Performance tests
    void testPerformance_largeDataset();
    
private:
    // Helper functions
    void setupTestDatabase();
    void insertTestAnime(int aid, const QString& title, const QString& type, 
                        int eptotal, bool is18Restricted);
    void insertTestMyListEntry(int aid, int normalEpisodes, int normalViewed,
                              int otherEpisodes, int otherViewed);
    void insertTestAlternativeTitle(int aid, const QString& title);
    
    // Test data verification
    bool animeExists(int aid);
    QString getAnimeType(int aid);
    bool isInMyList(int aid);
    bool is18Restricted(int aid);
    
    // Filter simulation functions
    bool matchesSearchFilter(int aid, const QString& title, const QString& searchText,
                            const QMap<int, QStringList>& altTitlesCache);
    bool matchesTypeFilter(const QString& animeType, const QString& filterType);
    bool matchesCompletionFilter(int normalViewed, int normalEpisodes, int totalEpisodes,
                                const QString& completionFilter);
    bool matchesUnwatchedFilter(int normalEpisodes, int normalViewed,
                               int otherEpisodes, int otherViewed);
    bool matchesAdultContentFilter(bool is18Restricted, const QString& adultFilter);
    
    QSqlDatabase m_db;
    QString m_testDbPath;
};

void TestFilterSystem::initTestCase()
{
    // Create a temporary test database
    m_testDbPath = QDir::tempPath() + "/test_filter_system.db";
    
    // Remove old test database if exists
    if (QFile::exists(m_testDbPath)) {
        QFile::remove(m_testDbPath);
    }
    
    m_db = QSqlDatabase::addDatabase("QSQLITE", "test_filter_connection");
    m_db.setDatabaseName(m_testDbPath);
    
    QVERIFY2(m_db.open(), "Failed to open test database");
    
    setupTestDatabase();
}

void TestFilterSystem::cleanupTestCase()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    
    QSqlDatabase::removeDatabase("test_filter_connection");
    
    // Clean up test database file
    if (QFile::exists(m_testDbPath)) {
        QFile::remove(m_testDbPath);
    }
}

void TestFilterSystem::init()
{
    // Clear test data before each test
    QSqlQuery query(m_db);
    query.exec("DELETE FROM anime");
    query.exec("DELETE FROM mylist");
    query.exec("DELETE FROM anime_titles");
}

void TestFilterSystem::cleanup()
{
    // Nothing needed per-test
}

void TestFilterSystem::setupTestDatabase()
{
    QSqlQuery query(m_db);
    
    // Create anime table
    QVERIFY(query.exec(
        "CREATE TABLE IF NOT EXISTS anime ("
        "aid INTEGER PRIMARY KEY, "
        "nameromaji TEXT, "
        "type TEXT, "
        "eptotal INTEGER, "
        "is18restricted INTEGER DEFAULT 0"
        ")"
    ));
    
    // Create mylist table
    QVERIFY(query.exec(
        "CREATE TABLE IF NOT EXISTS mylist ("
        "lid INTEGER PRIMARY KEY AUTOINCREMENT, "
        "aid INTEGER, "
        "normal_episodes INTEGER DEFAULT 0, "
        "normal_viewed INTEGER DEFAULT 0, "
        "other_episodes INTEGER DEFAULT 0, "
        "other_viewed INTEGER DEFAULT 0"
        ")"
    ));
    
    // Create anime_titles table for alternative titles
    QVERIFY(query.exec(
        "CREATE TABLE IF NOT EXISTS anime_titles ("
        "aid INTEGER, "
        "type INTEGER, "
        "language TEXT, "
        "title TEXT"
        ")"
    ));
}

void TestFilterSystem::insertTestAnime(int aid, const QString& title, const QString& type,
                                      int eptotal, bool is18Restricted)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO anime (aid, nameromaji, type, eptotal, is18restricted) "
                 "VALUES (:aid, :title, :type, :eptotal, :is18)");
    query.bindValue(":aid", aid);
    query.bindValue(":title", title);
    query.bindValue(":type", type);
    query.bindValue(":eptotal", eptotal);
    query.bindValue(":is18", is18Restricted ? 1 : 0);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}

void TestFilterSystem::insertTestMyListEntry(int aid, int normalEpisodes, int normalViewed,
                                             int otherEpisodes, int otherViewed)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO mylist (aid, normal_episodes, normal_viewed, "
                 "other_episodes, other_viewed) "
                 "VALUES (:aid, :ne, :nv, :oe, :ov)");
    query.bindValue(":aid", aid);
    query.bindValue(":ne", normalEpisodes);
    query.bindValue(":nv", normalViewed);
    query.bindValue(":oe", otherEpisodes);
    query.bindValue(":ov", otherViewed);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}

void TestFilterSystem::insertTestAlternativeTitle(int aid, const QString& title)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO anime_titles (aid, type, language, title) "
                 "VALUES (:aid, 1, 'en', :title)");
    query.bindValue(":aid", aid);
    query.bindValue(":title", title);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}

bool TestFilterSystem::animeExists(int aid)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM anime WHERE aid = :aid");
    query.bindValue(":aid", aid);
    query.exec();
    return query.next() && query.value(0).toInt() > 0;
}

QString TestFilterSystem::getAnimeType(int aid)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT type FROM anime WHERE aid = :aid");
    query.bindValue(":aid", aid);
    query.exec();
    if (query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

bool TestFilterSystem::isInMyList(int aid)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM mylist WHERE aid = :aid");
    query.bindValue(":aid", aid);
    query.exec();
    return query.next() && query.value(0).toInt() > 0;
}

bool TestFilterSystem::is18Restricted(int aid)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT is18restricted FROM anime WHERE aid = :aid");
    query.bindValue(":aid", aid);
    query.exec();
    if (query.next()) {
        return query.value(0).toInt() != 0;
    }
    return false;
}

// Simulate Window::matchesSearchFilter
bool TestFilterSystem::matchesSearchFilter(int aid, const QString& title,
                                          const QString& searchText,
                                          const QMap<int, QStringList>& altTitlesCache)
{
    if (searchText.isEmpty()) {
        return true;
    }
    
    // Check main title
    if (title.contains(searchText, Qt::CaseInsensitive)) {
        return true;
    }
    
    // Check alternative titles
    if (altTitlesCache.contains(aid)) {
        const QStringList& altTitles = altTitlesCache[aid];
        for (const QString& altTitle : altTitles) {
            if (altTitle.contains(searchText, Qt::CaseInsensitive)) {
                return true;
            }
        }
    }
    
    return false;
}

bool TestFilterSystem::matchesTypeFilter(const QString& animeType, const QString& filterType)
{
    if (filterType.isEmpty()) {
        return true;
    }
    return animeType == filterType;
}

bool TestFilterSystem::matchesCompletionFilter(int normalViewed, int normalEpisodes,
                                              int totalEpisodes, const QString& completionFilter)
{
    if (completionFilter.isEmpty()) {
        return true;
    }
    
    if (completionFilter == "completed") {
        if (totalEpisodes > 0) {
            return normalViewed >= totalEpisodes;
        } else {
            return normalEpisodes > 0 && normalViewed >= normalEpisodes;
        }
    } else if (completionFilter == "watching") {
        if (totalEpisodes > 0) {
            return normalViewed > 0 && normalViewed < totalEpisodes;
        } else {
            return normalViewed > 0 && normalViewed < normalEpisodes;
        }
    } else if (completionFilter == "notstarted") {
        return normalViewed == 0;
    }
    
    return false;
}

bool TestFilterSystem::matchesUnwatchedFilter(int normalEpisodes, int normalViewed,
                                             int otherEpisodes, int otherViewed)
{
    return (normalEpisodes > normalViewed) || (otherEpisodes > otherViewed);
}

bool TestFilterSystem::matchesAdultContentFilter(bool is18Restricted, const QString& adultFilter)
{
    if (adultFilter == "hide") {
        return !is18Restricted;
    } else if (adultFilter == "showonly") {
        return is18Restricted;
    }
    // "ignore" means no filtering
    return true;
}

// ============================================================================
// SEARCH FILTER TESTS
// ============================================================================

void TestFilterSystem::testSearchFilter_exactMatch()
{
    insertTestAnime(1, "Cowboy Bebop", "TV Series", 26, false);
    
    QMap<int, QStringList> altTitles;
    
    QVERIFY(matchesSearchFilter(1, "Cowboy Bebop", "Cowboy Bebop", altTitles));
}

void TestFilterSystem::testSearchFilter_partialMatch()
{
    insertTestAnime(1, "Cowboy Bebop", "TV Series", 26, false);
    
    QMap<int, QStringList> altTitles;
    
    QVERIFY(matchesSearchFilter(1, "Cowboy Bebop", "Cowboy", altTitles));
    QVERIFY(matchesSearchFilter(1, "Cowboy Bebop", "Bebop", altTitles));
    QVERIFY(matchesSearchFilter(1, "Cowboy Bebop", "boy Be", altTitles));
}

void TestFilterSystem::testSearchFilter_caseInsensitive()
{
    insertTestAnime(1, "Cowboy Bebop", "TV Series", 26, false);
    
    QMap<int, QStringList> altTitles;
    
    QVERIFY(matchesSearchFilter(1, "Cowboy Bebop", "cowboy", altTitles));
    QVERIFY(matchesSearchFilter(1, "Cowboy Bebop", "COWBOY", altTitles));
    QVERIFY(matchesSearchFilter(1, "Cowboy Bebop", "CowBoy BeBop", altTitles));
    QVERIFY(matchesSearchFilter(1, "Cowboy Bebop", "bebop", altTitles));
}

void TestFilterSystem::testSearchFilter_alternativeTitles()
{
    insertTestAnime(1, "Shin Seiki Evangelion", "TV Series", 26, false);
    insertTestAlternativeTitle(1, "Neon Genesis Evangelion");
    insertTestAlternativeTitle(1, "Evangelion");
    
    QMap<int, QStringList> altTitles;
    altTitles[1] = QStringList{"Neon Genesis Evangelion", "Evangelion"};
    
    // Should match main title
    QVERIFY(matchesSearchFilter(1, "Shin Seiki Evangelion", "Shin", altTitles));
    
    // Should match alternative titles
    QVERIFY(matchesSearchFilter(1, "Shin Seiki Evangelion", "Neon", altTitles));
    QVERIFY(matchesSearchFilter(1, "Shin Seiki Evangelion", "Genesis", altTitles));
    QVERIFY(matchesSearchFilter(1, "Shin Seiki Evangelion", "Evangelion", altTitles));
}

void TestFilterSystem::testSearchFilter_emptySearch()
{
    insertTestAnime(1, "Any Title", "TV Series", 12, false);
    
    QMap<int, QStringList> altTitles;
    
    // Empty search should match all
    QVERIFY(matchesSearchFilter(1, "Any Title", "", altTitles));
}

void TestFilterSystem::testSearchFilter_noMatch()
{
    insertTestAnime(1, "Cowboy Bebop", "TV Series", 26, false);
    
    QMap<int, QStringList> altTitles;
    altTitles[1] = QStringList{"Space Cowboys"};
    
    QVERIFY(!matchesSearchFilter(1, "Cowboy Bebop", "Gundam", altTitles));
    QVERIFY(!matchesSearchFilter(1, "Cowboy Bebop", "xyz123", altTitles));
}

// ============================================================================
// TYPE FILTER TESTS
// ============================================================================

void TestFilterSystem::testTypeFilter_tvSeries()
{
    QVERIFY(matchesTypeFilter("TV Series", "TV Series"));
    QVERIFY(!matchesTypeFilter("Movie", "TV Series"));
    QVERIFY(!matchesTypeFilter("OVA", "TV Series"));
}

void TestFilterSystem::testTypeFilter_movie()
{
    QVERIFY(matchesTypeFilter("Movie", "Movie"));
    QVERIFY(!matchesTypeFilter("TV Series", "Movie"));
    QVERIFY(!matchesTypeFilter("OVA", "Movie"));
}

void TestFilterSystem::testTypeFilter_ova()
{
    QVERIFY(matchesTypeFilter("OVA", "OVA"));
    QVERIFY(!matchesTypeFilter("TV Series", "OVA"));
    QVERIFY(!matchesTypeFilter("Movie", "OVA"));
}

void TestFilterSystem::testTypeFilter_noFilter()
{
    // Empty filter should match all types
    QVERIFY(matchesTypeFilter("TV Series", ""));
    QVERIFY(matchesTypeFilter("Movie", ""));
    QVERIFY(matchesTypeFilter("OVA", ""));
    QVERIFY(matchesTypeFilter("Web", ""));
}

// ============================================================================
// COMPLETION FILTER TESTS
// ============================================================================

void TestFilterSystem::testCompletionFilter_completed()
{
    // Completed with known total
    QVERIFY(matchesCompletionFilter(26, 26, 26, "completed"));  // All watched
    QVERIFY(matchesCompletionFilter(27, 26, 26, "completed"));  // Over-watched
    QVERIFY(!matchesCompletionFilter(25, 26, 26, "completed")); // Not all watched
    
    // Completed with unknown total (totalEpisodes = 0)
    QVERIFY(matchesCompletionFilter(12, 12, 0, "completed"));   // All in mylist watched
    QVERIFY(!matchesCompletionFilter(11, 12, 0, "completed"));  // Not all watched
    QVERIFY(!matchesCompletionFilter(0, 0, 0, "completed"));    // No episodes
}

void TestFilterSystem::testCompletionFilter_watching()
{
    // Watching with known total
    QVERIFY(matchesCompletionFilter(10, 26, 26, "watching"));   // Some watched
    QVERIFY(matchesCompletionFilter(1, 26, 26, "watching"));    // Just started
    QVERIFY(!matchesCompletionFilter(0, 26, 26, "watching"));   // None watched
    QVERIFY(!matchesCompletionFilter(26, 26, 26, "watching"));  // All watched
    
    // Watching with unknown total (totalEpisodes = 0)
    QVERIFY(matchesCompletionFilter(5, 12, 0, "watching"));     // Some watched
    QVERIFY(!matchesCompletionFilter(0, 12, 0, "watching"));    // None watched
    QVERIFY(!matchesCompletionFilter(12, 12, 0, "watching"));   // All watched
}

void TestFilterSystem::testCompletionFilter_notStarted()
{
    QVERIFY(matchesCompletionFilter(0, 26, 26, "notstarted"));  // Not started
    QVERIFY(!matchesCompletionFilter(1, 26, 26, "notstarted")); // Started
    QVERIFY(!matchesCompletionFilter(26, 26, 26, "notstarted"));// Completed
    
    // Works same way with unknown total
    QVERIFY(matchesCompletionFilter(0, 12, 0, "notstarted"));
    QVERIFY(!matchesCompletionFilter(1, 12, 0, "notstarted"));
}

void TestFilterSystem::testCompletionFilter_unknownTotal()
{
    // When total episodes is 0 (unknown), use normalEpisodes instead
    
    // Completed: all in mylist are viewed
    QVERIFY(matchesCompletionFilter(5, 5, 0, "completed"));
    QVERIFY(!matchesCompletionFilter(4, 5, 0, "completed"));
    
    // Watching: some but not all in mylist are viewed
    QVERIFY(matchesCompletionFilter(3, 5, 0, "watching"));
    QVERIFY(!matchesCompletionFilter(0, 5, 0, "watching"));
    QVERIFY(!matchesCompletionFilter(5, 5, 0, "watching"));
}

void TestFilterSystem::testCompletionFilter_noEpisodes()
{
    // Edge case: anime with no episodes
    QVERIFY(!matchesCompletionFilter(0, 0, 0, "completed"));  // Can't be completed with no episodes
    QVERIFY(!matchesCompletionFilter(0, 0, 0, "watching"));   // Can't be watching
    QVERIFY(matchesCompletionFilter(0, 0, 0, "notstarted"));  // Is not started
}

// ============================================================================
// UNWATCHED FILTER TESTS
// ============================================================================

void TestFilterSystem::testUnwatchedFilter_hasUnwatchedNormal()
{
    // Has unwatched normal episodes
    QVERIFY(matchesUnwatchedFilter(26, 10, 0, 0));  // 16 unwatched normal
    QVERIFY(matchesUnwatchedFilter(26, 0, 0, 0));   // All unwatched
    QVERIFY(matchesUnwatchedFilter(26, 25, 0, 0));  // 1 unwatched
}

void TestFilterSystem::testUnwatchedFilter_hasUnwatchedOther()
{
    // Has unwatched other episodes
    QVERIFY(matchesUnwatchedFilter(0, 0, 5, 2));    // 3 unwatched other
    QVERIFY(matchesUnwatchedFilter(0, 0, 5, 0));    // All other unwatched
}

void TestFilterSystem::testUnwatchedFilter_allWatched()
{
    // All episodes watched
    QVERIFY(!matchesUnwatchedFilter(26, 26, 5, 5)); // All normal and other watched
    QVERIFY(!matchesUnwatchedFilter(26, 26, 0, 0)); // All normal watched, no other
    QVERIFY(!matchesUnwatchedFilter(0, 0, 5, 5));   // No normal, all other watched
}

void TestFilterSystem::testUnwatchedFilter_noneWatched()
{
    // None watched means all unwatched
    QVERIFY(matchesUnwatchedFilter(26, 0, 5, 0));   // All unwatched
}

// ============================================================================
// ADULT CONTENT FILTER TESTS
// ============================================================================

void TestFilterSystem::testAdultContentFilter_hide()
{
    // Hide 18+ content (default)
    QVERIFY(matchesAdultContentFilter(false, "hide"));  // Show non-18+
    QVERIFY(!matchesAdultContentFilter(true, "hide"));  // Hide 18+
}

void TestFilterSystem::testAdultContentFilter_showOnly()
{
    // Show only 18+ content
    QVERIFY(!matchesAdultContentFilter(false, "showonly")); // Hide non-18+
    QVERIFY(matchesAdultContentFilter(true, "showonly"));   // Show 18+
}

void TestFilterSystem::testAdultContentFilter_ignore()
{
    // Ignore adult content filter
    QVERIFY(matchesAdultContentFilter(false, "ignore"));    // Show non-18+
    QVERIFY(matchesAdultContentFilter(true, "ignore"));     // Show 18+
}

// ============================================================================
// IN MYLIST FILTER TESTS
// ============================================================================

void TestFilterSystem::testInMyListFilter_onlyMyList()
{
    insertTestAnime(1, "In MyList", "TV Series", 26, false);
    insertTestAnime(2, "Not In MyList", "TV Series", 12, false);
    
    insertTestMyListEntry(1, 26, 10, 0, 0);
    
    QVERIFY(isInMyList(1));
    QVERIFY(!isInMyList(2));
}

void TestFilterSystem::testInMyListFilter_allAnime()
{
    insertTestAnime(1, "Anime 1", "TV Series", 26, false);
    insertTestAnime(2, "Anime 2", "Movie", 1, false);
    insertTestAnime(3, "Anime 3", "OVA", 6, false);
    
    insertTestMyListEntry(1, 26, 0, 0, 0);
    
    // When showing all anime, all should be visible
    QVERIFY(animeExists(1));
    QVERIFY(animeExists(2));
    QVERIFY(animeExists(3));
    
    // But only anime 1 is in mylist
    QVERIFY(isInMyList(1));
    QVERIFY(!isInMyList(2));
    QVERIFY(!isInMyList(3));
}

// ============================================================================
// FILTER COMBINATION TESTS
// ============================================================================

void TestFilterSystem::testCombination_typeAndCompletion()
{
    // Test combining type and completion filters
    insertTestAnime(1, "TV Show Completed", "TV Series", 26, false);
    insertTestAnime(2, "TV Show Watching", "TV Series", 26, false);
    insertTestAnime(3, "Movie Completed", "Movie", 1, false);
    
    insertTestMyListEntry(1, 26, 26, 0, 0);  // Completed
    insertTestMyListEntry(2, 26, 10, 0, 0);  // Watching
    insertTestMyListEntry(3, 1, 1, 0, 0);    // Completed
    
    // TV Series + Completed
    QVERIFY(matchesTypeFilter("TV Series", "TV Series") &&
           matchesCompletionFilter(26, 26, 26, "completed"));
    
    // Movie + Completed
    QVERIFY(matchesTypeFilter("Movie", "Movie") &&
           matchesCompletionFilter(1, 1, 1, "completed"));
    
    // TV Series + Watching
    QVERIFY(matchesTypeFilter("TV Series", "TV Series") &&
           matchesCompletionFilter(10, 26, 26, "watching"));
    
    // Movie + Watching (should fail - movie is completed)
    QVERIFY(!(matchesTypeFilter("Movie", "Movie") &&
             matchesCompletionFilter(1, 1, 1, "watching")));
}

void TestFilterSystem::testCombination_searchAndType()
{
    insertTestAnime(1, "Cowboy Bebop", "TV Series", 26, false);
    insertTestAnime(2, "Cowboy Bebop: The Movie", "Movie", 1, false);
    
    QMap<int, QStringList> altTitles;
    
    // Search "Cowboy" + Type "TV Series"
    QVERIFY(matchesSearchFilter(1, "Cowboy Bebop", "Cowboy", altTitles) &&
           matchesTypeFilter("TV Series", "TV Series"));
    
    // Search "Cowboy" + Type "Movie"
    QVERIFY(matchesSearchFilter(2, "Cowboy Bebop: The Movie", "Cowboy", altTitles) &&
           matchesTypeFilter("Movie", "Movie"));
    
    // Search "Gundam" + Type "TV Series" (no match)
    QVERIFY(!(matchesSearchFilter(1, "Cowboy Bebop", "Gundam", altTitles) &&
             matchesTypeFilter("TV Series", "TV Series")));
}

void TestFilterSystem::testCombination_allFiltersActive()
{
    // Test with all filters active at once
    insertTestAnime(1, "Test Anime", "TV Series", 26, false);
    insertTestAlternativeTitle(1, "Alternative Title");
    insertTestMyListEntry(1, 26, 10, 0, 0);  // Watching, has unwatched
    
    QMap<int, QStringList> altTitles;
    altTitles[1] = QStringList{"Alternative Title"};
    
    // All filters should pass
    bool passes = matchesSearchFilter(1, "Test Anime", "Test", altTitles) &&
                 matchesTypeFilter("TV Series", "TV Series") &&
                 matchesCompletionFilter(10, 26, 26, "watching") &&
                 matchesUnwatchedFilter(26, 10, 0, 0) &&
                 matchesAdultContentFilter(false, "hide");
    
    QVERIFY(passes);
}

void TestFilterSystem::testCombination_multipleAnime()
{
    // Test filtering with multiple anime
    insertTestAnime(1, "Anime A", "TV Series", 26, false);
    insertTestAnime(2, "Anime B", "Movie", 1, false);
    insertTestAnime(3, "Anime C", "OVA", 6, true);  // 18+
    insertTestAnime(4, "Anime D", "TV Series", 12, false);
    
    insertTestMyListEntry(1, 26, 26, 0, 0);  // Completed
    insertTestMyListEntry(2, 1, 0, 0, 0);    // Not started
    insertTestMyListEntry(3, 6, 3, 0, 0);    // Watching
    insertTestMyListEntry(4, 12, 5, 0, 0);   // Watching
    
    // Count how many match "TV Series + Watching"
    int matchCount = 0;
    
    if (matchesTypeFilter("TV Series", "TV Series") &&
        matchesCompletionFilter(26, 26, 26, "watching"))
        matchCount++;
    
    if (matchesTypeFilter("Movie", "TV Series") &&
        matchesCompletionFilter(0, 1, 1, "watching"))
        matchCount++;
    
    if (matchesTypeFilter("OVA", "TV Series") &&
        matchesCompletionFilter(3, 6, 6, "watching"))
        matchCount++;
    
    if (matchesTypeFilter("TV Series", "TV Series") &&
        matchesCompletionFilter(5, 12, 12, "watching"))
        matchCount++;
    
    // Only anime 4 should match (TV Series + Watching)
    QCOMPARE(matchCount, 1);
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

void TestFilterSystem::testEdgeCase_emptyDatabase()
{
    // Verify database is empty
    QSqlQuery query(m_db);
    query.exec("SELECT COUNT(*) FROM anime");
    query.next();
    QCOMPARE(query.value(0).toInt(), 0);
    
    // Filters should handle empty data gracefully
    QMap<int, QStringList> altTitles;
    
    // These shouldn't crash
    matchesSearchFilter(999, "Nonexistent", "test", altTitles);
    matchesTypeFilter("TV Series", "TV Series");
    matchesCompletionFilter(0, 0, 0, "completed");
}

void TestFilterSystem::testEdgeCase_missingData()
{
    insertTestAnime(1, "Anime", "TV Series", 26, false);
    // Don't add mylist entry
    
    QVERIFY(animeExists(1));
    QVERIFY(!isInMyList(1));
    
    // Filters should handle missing mylist data
    // (in real app, this would be caught by "In MyList" filter first)
}

void TestFilterSystem::testEdgeCase_zeroEpisodes()
{
    insertTestAnime(1, "Special", "TV Special", 0, false);
    insertTestMyListEntry(1, 0, 0, 0, 0);
    
    // Should handle zero episodes without crashing
    QVERIFY(!matchesCompletionFilter(0, 0, 0, "completed"));
    QVERIFY(!matchesCompletionFilter(0, 0, 0, "watching"));
    QVERIFY(matchesCompletionFilter(0, 0, 0, "notstarted"));
    
    // Unwatched filter with zero episodes
    QVERIFY(!matchesUnwatchedFilter(0, 0, 0, 0));
}

// ============================================================================
// PERFORMANCE TEST
// ============================================================================

void TestFilterSystem::testPerformance_largeDataset()
{
    // Insert 1000 test anime
    for (int i = 1; i <= 1000; i++) {
        QString title = QString("Anime %1").arg(i);
        QString type = (i % 3 == 0) ? "Movie" : "TV Series";
        bool is18 = (i % 10 == 0);
        
        insertTestAnime(i, title, type, 26, is18);
        insertTestMyListEntry(i, 26, i % 27, 0, 0);  // Varied completion
        insertTestAlternativeTitle(i, QString("Alt Title %1").arg(i));
    }
    
    // Verify data inserted
    QSqlQuery query(m_db);
    query.exec("SELECT COUNT(*) FROM anime");
    query.next();
    QCOMPARE(query.value(0).toInt(), 1000);
    
    QMap<int, QStringList> altTitles;
    for (int i = 1; i <= 1000; i++) {
        altTitles[i] = QStringList{QString("Alt Title %1").arg(i)};
    }
    
    // Time the filtering operation
    QElapsedTimer timer;
    timer.start();
    
    int matchCount = 0;
    for (int i = 1; i <= 1000; i++) {
        QString title = QString("Anime %1").arg(i);
        QString type = (i % 3 == 0) ? "Movie" : "TV Series";
        bool is18 = (i % 10 == 0);
        int normalViewed = i % 27;
        
        bool matches = matchesSearchFilter(i, title, "Anime", altTitles) &&
                      matchesTypeFilter(type, "TV Series") &&
                      matchesCompletionFilter(normalViewed, 26, 26, "watching") &&
                      matchesAdultContentFilter(is18, "hide");
        
        if (matches) matchCount++;
    }
    
    qint64 elapsed = timer.elapsed();
    
    // Should complete in reasonable time (< 100ms for 1000 items)
    QVERIFY2(elapsed < 100, qPrintable(QString("Filtering took %1ms").arg(elapsed)));
    
    // Verify some matches were found
    QVERIFY(matchCount > 0);
}

QTEST_MAIN(TestFilterSystem)
#include "test_filter_system.moc"
