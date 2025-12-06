#include "localfileinfo.h"
#include <QFile>
#include <QRegularExpression>

// Initialize static members
const QStringList LocalFileInfo::VIDEO_EXTENSIONS = {
    "mkv", "mp4", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg", "ts", "m2ts"
};

const QStringList LocalFileInfo::SUBTITLE_EXTENSIONS = {
    "srt", "ass", "ssa", "sub", "idx", "vtt"
};

const QStringList LocalFileInfo::AUDIO_EXTENSIONS = {
    "mp3", "flac", "wav", "aac", "ogg", "m4a", "wma", "opus"
};

LocalFileInfo::LocalFileInfo()
    : m_size(0)
    , m_selectedAid(0)
    , m_selectedEid(0)
{
}

LocalFileInfo::LocalFileInfo(const QString& filename, const QString& filepath, 
                             const QString& hash, qint64 size)
    : m_filename(filename)
    , m_filepath(filepath)
    , m_hash(hash)
    , m_size(size)
    , m_selectedAid(0)
    , m_selectedEid(0)
{
    // Validate and normalize hash if provided
    if (!hash.isEmpty()) {
        setHash(hash);
    }
}

LocalFileInfo::LocalFileInfo(const QFileInfo& fileInfo, const QString& hash)
    : m_filename(fileInfo.fileName())
    , m_filepath(fileInfo.absoluteFilePath())
    , m_hash(hash)
    , m_size(fileInfo.size())
    , m_selectedAid(0)
    , m_selectedEid(0)
{
    // Validate and normalize hash if provided
    if (!hash.isEmpty()) {
        setHash(hash);
    }
}

void LocalFileInfo::setFilepath(const QString& filepath)
{
    m_filepath = filepath;
    
    // Auto-update filename if filepath changes
    if (!filepath.isEmpty()) {
        QFileInfo info(filepath);
        m_filename = info.fileName();
        
        // Update size if file exists
        if (info.exists()) {
            m_size = info.size();
        }
    }
}

void LocalFileInfo::setHash(const QString& hash)
{
    // Normalize hash to lowercase and remove any whitespace
    QString normalized = hash.trimmed().toLower();
    
    // Only set if valid
    if (normalized.isEmpty() || isValidHash(normalized)) {
        m_hash = normalized;
    }
}

bool LocalFileInfo::exists() const
{
    return !m_filepath.isEmpty() && QFile::exists(m_filepath);
}

QString LocalFileInfo::absolutePath() const
{
    QFileInfo info(m_filepath);
    return info.absoluteFilePath();
}

QString LocalFileInfo::directory() const
{
    QFileInfo info(m_filepath);
    return info.absolutePath();
}

QString LocalFileInfo::extension() const
{
    QFileInfo info(m_filepath);
    return info.suffix().toLower();
}

QString LocalFileInfo::baseName() const
{
    QFileInfo info(m_filepath);
    return info.completeBaseName();
}

bool LocalFileInfo::isVideoFile() const
{
    QString ext = extension();
    return VIDEO_EXTENSIONS.contains(ext, Qt::CaseInsensitive);
}

bool LocalFileInfo::isSubtitleFile() const
{
    QString ext = extension();
    return SUBTITLE_EXTENSIONS.contains(ext, Qt::CaseInsensitive);
}

bool LocalFileInfo::isAudioFile() const
{
    QString ext = extension();
    return AUDIO_EXTENSIONS.contains(ext, Qt::CaseInsensitive);
}

bool LocalFileInfo::isValidHash(const QString& hash)
{
    // ED2K hash is 32 character hex string
    static const QRegularExpression hashRegex("^[0-9a-f]{32}$", QRegularExpression::CaseInsensitiveOption);
    return hashRegex.match(hash.trimmed()).hasMatch();
}

bool LocalFileInfo::operator==(const LocalFileInfo& other) const
{
    // Compare based on absolute filepath for uniqueness
    QFileInfo thisInfo(m_filepath);
    QFileInfo otherInfo(other.m_filepath);
    return thisInfo.absoluteFilePath() == otherInfo.absoluteFilePath();
}

bool LocalFileInfo::operator!=(const LocalFileInfo& other) const
{
    return !(*this == other);
}
