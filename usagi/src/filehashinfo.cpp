#include "filehashinfo.h"
#include <QRegularExpression>

FileHashInfo::FileHashInfo()
    : m_status(0)
    , m_bindingStatus(0)
{
}

FileHashInfo::FileHashInfo(const QString& path, const QString& hash, int status, int bindingStatus)
    : m_path(path)
    , m_hash(hash)
    , m_status(status)
    , m_bindingStatus(bindingStatus)
{
}

void FileHashInfo::setPath(const QString& path)
{
    m_path = path;
}

void FileHashInfo::setHash(const QString& hash)
{
    m_hash = hash;
}

bool FileHashInfo::isValid() const
{
    return !m_path.isEmpty() && !m_hash.isEmpty();
}

bool FileHashInfo::hasValidHash() const
{
    return !m_hash.isEmpty() && isValidHexHash(m_hash);
}

void FileHashInfo::reset()
{
    m_path.clear();
    m_hash.clear();
    m_status = 0;
    m_bindingStatus = 0;
}

bool FileHashInfo::isValidHexHash(const QString& hash)
{
    if (hash.isEmpty()) {
        return false;
    }
    
    // ED2K hashes are ED2K_HASH_LENGTH characters (128 bits) in hexadecimal
    static const QRegularExpression hexPattern(QStringLiteral("^[0-9a-fA-F]{32}$"));
    return hexPattern.match(hash).hasMatch();
}
