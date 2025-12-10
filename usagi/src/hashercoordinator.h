#ifndef HASHERCOORDINATOR_H
#define HASHERCOORDINATOR_H

#include <QObject>
#include <QWidget>
#include <QBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QVector>
#include <QTimer>
#include <QMutex>
#include <QFileInfo>
#include <QStringList>
#include <QList>
#include <QRegularExpression>
#include <QTableWidget>
#include <QUrl>
#include "hash/ed2k.h"
#include "hashingtask.h"
#include "progresstracker.h"

// Forward declarations
class AniDBApi;
class HasherThreadPool;
class hashes_;

/**
 * HasherCoordinator - Manages the file hashing UI and coordination
 * 
 * This class provides:
 * - File selection (individual files, directories)
 * - Hasher UI management (progress bars, output, controls)
 * - Coordination with HasherThreadPool
 * - Progress tracking and display
 * - File filtering based on masks
 * 
 * Design principles:
 * - Single Responsibility: Only handles hasher coordination
 * - Decoupled from main Window class
 * - Clear interface for communication via signals
 */
class HasherCoordinator : public QObject
{
    Q_OBJECT
    
public:
    explicit HasherCoordinator(AniDBApi *adbapi, QWidget *parent = nullptr);
    virtual ~HasherCoordinator();
    
    // Get the hasher page widget to add to Window's tab widget
    QWidget* getHasherPageWidget() const { return m_pageHasherParent; }
    
    // Get the hashes table widget for external access
    hashes_* getHashesTable() const { return m_hashes; }
    
    // Get hasher output widget
    QTextEdit* getHasherOutput() const { return m_hasherOutput; }
    
    // Get hasher settings layout
    QGridLayout* getHasherSettings() const { return m_pageHasherSettings; }
    
    // Get progress bars
    QVector<QProgressBar*> getThreadProgressBars() const { return m_threadProgressBars; }
    QProgressBar* getTotalProgressBar() const { return m_progressTotal; }
    QLabel* getTotalProgressLabel() const { return m_progressTotalLabel; }
    
    // Public accessors for UI widgets (needed by Window for backward compatibility)
    QCheckBox* getRenameTo() const { return m_renameTo; }
    QPushButton* getButtonStart() const { return m_buttonStart; }
    QPushButton* getButtonClear() const { return m_buttonClear; }
    QCheckBox* getMarkWatched() const { return m_markWatched; }
    QComboBox* getHasherFileState() const { return m_hasherFileState; }
    QLineEdit* getStorage() const { return m_storage; }
    
    // Public methods for file management (needed by Window)
    bool shouldFilterFile(const QString &filePath);
    void hashesInsertRow(QFileInfo file, Qt::CheckState renameState, const QString& preloadedHash = QString());
    QStringList getFilesNeedingHash();
    void setupHashingProgress(const QStringList &files);
    
signals:
    // Signal to notify when hashing is finished
    void hashingFinished();
    
    // Signal for log messages
    void logMessage(const QString &message);
    
public slots:
    // File selection slots
    void addFiles();
    void addDirectories();
    void addLastDirectory();
    
    // Hasher control slots
    void startHashing();
    void stopHashing();
    void clearHasher();
    
    // Notification slots from hasher threads
    void onFileHashed(int threadId, ed2k::ed2kfilestruct fileData);
    void onProgressUpdate(int threadId, int total, int done);
    void onHashingFinished();
    
    // File provisioning for hasher threads
    void provideNextFileToHash();
    
    // Settings change handlers
    void onMarkWatchedStateChanged(Qt::CheckState state);
    
private:
    // UI Creation
    void createUI(QWidget *parent);
    void setupConnections();
    
    // Helper methods
    void addFilesFromDirectory(const QString &dirPath);
    void updateFilterCache();
    int calculateTotalHashParts(const QStringList &files);
    void processPendingHashedFiles();
    
    // UI Widgets
    QWidget *m_pageHasherParent;
    QBoxLayout *m_pageHasher;
    QBoxLayout *m_pageHasherAddButtons;
    QGridLayout *m_pageHasherSettings;
    QWidget *m_pageHasherAddButtonsParent;
    QWidget *m_pageHasherSettingsParent;
    
    QPushButton *m_button1;  // Add files
    QPushButton *m_button2;  // Add directories
    QPushButton *m_button3;  // Add last directory
    QTextEdit *m_hasherOutput;
    QPushButton *m_buttonStart;
    QPushButton *m_buttonStop;
    QPushButton *m_buttonClear;
    QVector<QProgressBar*> m_threadProgressBars;
    QProgressBar *m_progressTotal;
    QLabel *m_progressTotalLabel;
    QCheckBox *m_addToMyList;
    QCheckBox *m_markWatched;
    QLineEdit *m_storage;
    QComboBox *m_hasherFileState;
    QCheckBox *m_moveTo;
    QCheckBox *m_renameTo;
    QLineEdit *m_moveToDir;
    QLineEdit *m_renameToPattern;
    
    hashes_ *m_hashes;  // Hash table widget
    
    // Reference to AniDBApi (not owned)
    AniDBApi *m_adbapi;
    
    // Progress tracking
    ProgressTracker m_hashingProgress;
    int m_totalHashParts;
    int m_completedHashParts;
    QMap<int, int> m_lastThreadProgress;
    
    // File management
    QMutex m_fileRequestMutex;
    
    // Deferred processing
    static const int HASHED_FILES_BATCH_SIZE = 5;
    static const int HASHED_FILES_TIMER_INTERVAL = 10;
    QList<HashingTask> m_pendingHashedFilesQueue;
    QTimer *m_hashedFilesProcessingTimer;
    QColor m_hashedFileColor;
    
    // Filter cache
    QString m_cachedFilterMasks;
    QList<QRegularExpression> m_cachedFilterRegexes;
    
    // Reference to HasherThreadPool (not owned, accessed via extern)
    HasherThreadPool *m_hasherThreadPool;
};

#endif // HASHERCOORDINATOR_H
