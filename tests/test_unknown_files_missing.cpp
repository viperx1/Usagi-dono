#include <QTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QFile>
#include "../usagi/src/unknownfilesmanager.h"
#include "../usagi/src/anidbapi.h"
#include "../usagi/src/hashercoordinator.h"
#include "../usagi/src/logger.h"

/**
 * Test to verify auto-removal of missing unknown files
 * 
 * This test ensures that:
 * 1. Missing files are automatically removed when detected during file operations
 * 2. The removeMissingFiles() method correctly identifies and removes missing files
 * 3. File existence checks work correctly
 */
class TestUnknownFilesMissing : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testRemoveMissingFilesEmpty();
    void testRemoveMissingFilesSingle();
    void testRemoveMissingFilesMultiple();
    void testRemoveMissingFilesMixed();

private:
    AniDBApi *m_api = nullptr;
    HasherCoordinator *m_hasherCoord = nullptr;
    UnknownFilesManager *m_manager = nullptr;
    QTemporaryDir *m_tempDir = nullptr;
};

void TestUnknownFilesMissing::initTestCase()
{
    // Initialize logger
    Logger::instance();
    
    // Create temporary directory for test files
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
    
    // Create mock API and hasher coordinator (nullptr is acceptable for these tests)
    m_api = nullptr;
    m_hasherCoord = nullptr;
    
    // Create manager
    m_manager = new UnknownFilesManager(m_api, m_hasherCoord);
}

void TestUnknownFilesMissing::cleanupTestCase()
{
    delete m_manager;
    delete m_tempDir;
}

void TestUnknownFilesMissing::testRemoveMissingFilesEmpty()
{
    // Test with no files in the unknown files list
    int removed = m_manager->removeMissingFiles();
    QCOMPARE(removed, 0);
}

void TestUnknownFilesMissing::testRemoveMissingFilesSingle()
{
    // Create a temporary file, add it to the manager, then delete it
    QTemporaryFile tempFile(m_tempDir->path() + "/XXXXXX.mkv");
    tempFile.setAutoRemove(false);
    QVERIFY(tempFile.open());
    
    QString filePath = tempFile.fileName();
    QString fileName = QFileInfo(filePath).fileName();
    tempFile.close();
    
    // Verify file exists
    QVERIFY(QFile::exists(filePath));
    
    // Add file to manager
    m_manager->insertFile(fileName, filePath, "testhash123", 1024);
    
    // Verify it was added
    QCOMPARE(m_manager->getTableWidget()->rowCount(), 1);
    
    // Delete the file
    QVERIFY(QFile::remove(filePath));
    QVERIFY(!QFile::exists(filePath));
    
    // Run removeMissingFiles
    int removed = m_manager->removeMissingFiles();
    
    // Verify the file was removed
    QCOMPARE(removed, 1);
    QCOMPARE(m_manager->getTableWidget()->rowCount(), 0);
}

void TestUnknownFilesMissing::testRemoveMissingFilesMultiple()
{
    // Create multiple temporary files, add them, then delete all
    QList<QString> filePaths;
    
    for (int i = 0; i < 3; ++i) {
        QTemporaryFile tempFile(m_tempDir->path() + "/XXXXXX.mkv");
        tempFile.setAutoRemove(false);
        QVERIFY(tempFile.open());
        
        QString filePath = tempFile.fileName();
        QString fileName = QFileInfo(filePath).fileName();
        filePaths.append(filePath);
        tempFile.close();
        
        // Add file to manager
        m_manager->insertFile(fileName, filePath, QString("testhash%1").arg(i), 1024);
    }
    
    // Verify all files were added
    QCOMPARE(m_manager->getTableWidget()->rowCount(), 3);
    
    // Delete all files
    for (const QString& filePath : filePaths) {
        QVERIFY(QFile::remove(filePath));
        QVERIFY(!QFile::exists(filePath));
    }
    
    // Run removeMissingFiles
    int removed = m_manager->removeMissingFiles();
    
    // Verify all files were removed
    QCOMPARE(removed, 3);
    QCOMPARE(m_manager->getTableWidget()->rowCount(), 0);
}

void TestUnknownFilesMissing::testRemoveMissingFilesMixed()
{
    // Create multiple files, add them, then delete only some
    QList<QString> allFilePaths;
    QList<QString> deletedFilePaths;
    
    // Create 5 files
    for (int i = 0; i < 5; ++i) {
        QTemporaryFile tempFile(m_tempDir->path() + "/XXXXXX.mkv");
        tempFile.setAutoRemove(false);
        QVERIFY(tempFile.open());
        
        QString filePath = tempFile.fileName();
        QString fileName = QFileInfo(filePath).fileName();
        allFilePaths.append(filePath);
        tempFile.close();
        
        // Add file to manager
        m_manager->insertFile(fileName, filePath, QString("testhash%1").arg(i), 1024);
        
        // Mark files 1 and 3 for deletion (0-indexed, so positions 1 and 3)
        if (i == 1 || i == 3) {
            deletedFilePaths.append(filePath);
        }
    }
    
    // Verify all files were added
    QCOMPARE(m_manager->getTableWidget()->rowCount(), 5);
    
    // Delete only the marked files
    for (const QString& filePath : deletedFilePaths) {
        QVERIFY(QFile::remove(filePath));
        QVERIFY(!QFile::exists(filePath));
    }
    
    // Run removeMissingFiles
    int removed = m_manager->removeMissingFiles();
    
    // Verify only the deleted files were removed
    QCOMPARE(removed, 2);
    QCOMPARE(m_manager->getTableWidget()->rowCount(), 3);
    
    // Clean up remaining files
    for (const QString& filePath : allFilePaths) {
        if (QFile::exists(filePath)) {
            QFile::remove(filePath);
        }
    }
}

QTEST_MAIN(TestUnknownFilesMissing)
#include "test_unknown_files_missing.moc"
