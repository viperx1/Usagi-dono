#ifndef LOCALFILEINFO_H
#define LOCALFILEINFO_H

#include <QString>
#include <QFileInfo>

/**
 * @brief LocalFileInfo - Encapsulates information about a local file
 * 
 * This class replaces the UnboundFileData struct that was defined twice
 * in the codebase. It provides:
 * - Type-safe access to file information
 * - Validation of file paths and hashes
 * - Utility methods for common operations
 * - Single definition used across the codebase
 * 
 * Design improvements over struct:
 * - Encapsulation: Data validation in constructor
 * - Single Responsibility: Represents a local file
 * - Type Safety: Proper getters with appropriate types
 * - Utility Methods: Common operations like file existence check
 * 
 * This class is used for:
 * - Unbound files (files not in AniDB database)
 * - Directory watcher file tracking
 * - Thread-safe file data passing
 */
class LocalFileInfo
{
public:
    /**
     * @brief Default constructor creates invalid file info
     */
    LocalFileInfo();
    
    /**
     * @brief Construct file info from components
     * @param filename Base filename without path
     * @param filepath Full path to the file
     * @param hash ED2K hash of the file (if available)
     * @param size File size in bytes
     */
    LocalFileInfo(const QString& filename, const QString& filepath, 
                  const QString& hash = QString(), qint64 size = 0);
    
    /**
     * @brief Construct file info from QFileInfo
     * @param fileInfo Qt file info object
     * @param hash ED2K hash (optional)
     */
    explicit LocalFileInfo(const QFileInfo& fileInfo, const QString& hash = QString());
    
    // Getters
    QString filename() const { return m_filename; }
    QString filepath() const { return m_filepath; }
    QString hash() const { return m_hash; }
    qint64 size() const { return m_size; }
    
    // Setters
    void setFilename(const QString& filename) { m_filename = filename; }
    void setFilepath(const QString& filepath);
    void setHash(const QString& hash);
    void setSize(qint64 size) { m_size = size; }
    
    // Validation
    bool isValid() const { return !m_filepath.isEmpty() && m_size >= 0; }
    bool hasHash() const { return !m_hash.isEmpty() && isValidHash(m_hash); }
    
    // File operations
    bool exists() const;
    QString absolutePath() const;
    QString directory() const;
    QString extension() const;
    QString baseName() const;  // Filename without extension
    
    // File type checks
    bool isVideoFile() const;
    bool isSubtitleFile() const;
    bool isAudioFile() const;
    
    // Hash validation
    static bool isValidHash(const QString& hash);
    
    // Equality comparison (based on filepath)
    bool operator==(const LocalFileInfo& other) const;
    bool operator!=(const LocalFileInfo& other) const;
    
private:
    QString m_filename;  // Base filename (e.g., "video.mkv")
    QString m_filepath;  // Full path (e.g., "/path/to/video.mkv")
    QString m_hash;      // ED2K hash (32 character hex string)
    qint64 m_size;       // File size in bytes
    
    // Common video file extensions
    static const QStringList VIDEO_EXTENSIONS;
    
    // Common subtitle file extensions
    static const QStringList SUBTITLE_EXTENSIONS;
    
    // Common audio file extensions
    static const QStringList AUDIO_EXTENSIONS;
};

#endif // LOCALFILEINFO_H
