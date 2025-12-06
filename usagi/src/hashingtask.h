#ifndef HASHINGTASK_H
#define HASHINGTASK_H

#include <QString>

/**
 * @brief HashingTask - Encapsulates information for a file hashing task
 * 
 * This class replaces the HashedFileInfo struct with:
 * - Proper encapsulation of file data and processing settings
 * - Validation of file paths and hashes
 * - Clear separation between file info and processing options
 * 
 * Follows SOLID principles:
 * - Single Responsibility: Represents a single hashing task with its configuration
 * - Encapsulation: Private members with validated setters
 * - Type Safety: Proper types and validation
 * 
 * Usage:
 *   HashingTask task;
 *   task.setFilePath("/path/to/file.mkv");
 *   task.setHash("abc123...");
 *   task.setAddToMylist(true);
 *   if (task.isValid()) {
 *       // Process task
 *   }
 */
class HashingTask
{
public:
    /**
     * @brief Default constructor creates invalid task
     */
    HashingTask();
    
    /**
     * @brief Construct task with file information
     * @param filePath Full path to the file
     * @param filename Base filename
     * @param hexdigest ED2K hash of the file
     * @param fileSize File size in bytes
     */
    HashingTask(const QString& filePath, const QString& filename, 
                const QString& hexdigest, qint64 fileSize);
    
    // File information getters
    int rowIndex() const { return m_rowIndex; }
    QString filePath() const { return m_filePath; }
    QString filename() const { return m_filename; }
    QString hash() const { return m_hexdigest; }
    qint64 fileSize() const { return m_fileSize; }
    
    // File information setters
    void setRowIndex(int index) { m_rowIndex = index; }
    void setFilePath(const QString& path);
    void setFilename(const QString& name) { m_filename = name; }
    void setHash(const QString& hash);
    void setFileSize(qint64 size) { m_fileSize = size; }
    
    // Processing options getters
    bool useUserSettings() const { return m_useUserSettings; }
    bool addToMylist() const { return m_addToMylist; }
    int markWatchedState() const { return m_markWatchedState; }
    int fileState() const { return m_fileState; }
    
    // Processing options setters
    void setUseUserSettings(bool use) { m_useUserSettings = use; }
    void setAddToMylist(bool add) { m_addToMylist = add; }
    void setMarkWatchedState(int state) { m_markWatchedState = state; }
    void setFileState(int state) { m_fileState = state; }
    
    // Validation
    bool isValid() const;
    bool hasHash() const;
    
    // Utility methods
    QString formatSize() const;  // Returns human-readable size (e.g., "1.5 GB")
    
private:
    // File information
    int m_rowIndex;
    QString m_filePath;
    QString m_filename;
    QString m_hexdigest;  // ED2K hash
    qint64 m_fileSize;
    
    // Processing options
    bool m_useUserSettings;   // If true, use UI settings; if false, use auto-watcher defaults
    bool m_addToMylist;       // Whether to add to mylist
    int m_markWatchedState;   // Used only when useUserSettings is true
    int m_fileState;          // Used only when useUserSettings is true
};

#endif // HASHINGTASK_H
