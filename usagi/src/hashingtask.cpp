#include "hashingtask.h"
#include <QFileInfo>

HashingTask::HashingTask()
    : m_rowIndex(-1)
    , m_fileSize(0)
    , m_useUserSettings(true)
    , m_addToMylist(false)
    , m_markWatchedState(0)
    , m_fileState(0)
{
}

HashingTask::HashingTask(const QString& filePath, const QString& filename, 
                         const QString& hexdigest, qint64 fileSize)
    : m_rowIndex(-1)
    , m_filePath(filePath)
    , m_filename(filename)
    , m_hexdigest(hexdigest)
    , m_fileSize(fileSize)
    , m_useUserSettings(true)
    , m_addToMylist(false)
    , m_markWatchedState(0)
    , m_fileState(0)
{
}

void HashingTask::setFilePath(const QString& path)
{
    m_filePath = path;
    
    // Auto-populate filename if not set
    if (m_filename.isEmpty() && !path.isEmpty()) {
        QFileInfo fileInfo(path);
        m_filename = fileInfo.fileName();
    }
}

void HashingTask::setHash(const QString& hash)
{
    // Validate hash format (should be 32 hex characters for ED2K)
    // Empty hash is allowed (file not yet hashed)
    if (!hash.isEmpty() && hash.length() != 32) {
        // Invalid hash format - do not set it
        return;
    }
    m_hexdigest = hash;
}

bool HashingTask::isValid() const
{
    return !m_filePath.isEmpty() && m_fileSize >= 0;
}

bool HashingTask::hasHash() const
{
    return !m_hexdigest.isEmpty() && m_hexdigest.length() == 32;
}

QString HashingTask::formatSize() const
{
    if (m_fileSize < 1024) {
        return QString("%1 B").arg(m_fileSize);
    } else if (m_fileSize < 1024 * 1024) {
        return QString("%1 KB").arg(m_fileSize / 1024.0, 0, 'f', 2);
    } else if (m_fileSize < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(m_fileSize / (1024.0 * 1024.0), 0, 'f', 2);
    } else {
        return QString("%1 GB").arg(m_fileSize / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
}
