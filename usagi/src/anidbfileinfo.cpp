#include "anidbfileinfo.h"
#include <QRegularExpression>

// Bit flags for file state field (from AniDB UDP API spec)
static constexpr int STATE_CRCOK = 0x01;
static constexpr int STATE_CRCERR = 0x02;
static constexpr int STATE_ISV2 = 0x04;
static constexpr int STATE_ISV3 = 0x08;
static constexpr int STATE_ISV4 = 0x10;
static constexpr int STATE_ISV5 = 0x20;
static constexpr int STATE_UNC = 0x40;
static constexpr int STATE_CEN = 0x80;

AniDBFileInfo::AniDBFileInfo()
    : m_fid(0)
    , m_aid(0)
    , m_eid(0)
    , m_gid(0)
    , m_lid(0)
    , m_size(0)
    , m_isdepr(false)
    , m_state(0)
    , m_bitrate_audio(0)
    , m_bitrate_video(0)
    , m_length(0)
{
}

AniDBFileInfo AniDBFileInfo::fromApiResponse(const QStringList& tokens, 
                                            unsigned int fmask, 
                                            int& index)
{
    AniDBFileInfo info;
    
    // Parse fields based on fmask bits (see anidbapi.h file_fmask_codes)
    // Bits are checked in order from MSB to LSB
    
    if (fmask & 0x40000000) info.setAnimeId(parseIntField(tokens, index));        // fAID
    if (fmask & 0x20000000) info.setEpisodeId(parseIntField(tokens, index));      // fEID
    if (fmask & 0x10000000) info.setGroupId(parseIntField(tokens, index));        // fGID
    if (fmask & 0x08000000) info.setMylistId(parseIntField(tokens, index));       // fLID
    if (fmask & 0x04000000) info.setOtherEpisodes(parseField(tokens, index));     // fOTHEREPS
    if (fmask & 0x02000000) info.setDeprecated(parseBoolField(tokens, index));    // fISDEPR
    if (fmask & 0x01000000) info.setState(parseIntField(tokens, index));          // fSTATE
    if (fmask & 0x00800000) info.setSize(parseInt64Field(tokens, index));         // fSIZE
    if (fmask & 0x00400000) info.setEd2kHash(parseField(tokens, index));          // fED2K
    if (fmask & 0x00200000) info.setMd5Hash(parseField(tokens, index));           // fMD5
    if (fmask & 0x00100000) info.setSha1Hash(parseField(tokens, index));          // fSHA1
    if (fmask & 0x00080000) info.setCrc32(parseField(tokens, index));             // fCRC32
    // Skip unused bits
    if (fmask & 0x00008000) info.setQuality(parseField(tokens, index));           // fQUALITY
    if (fmask & 0x00004000) info.setSource(parseField(tokens, index));            // fSOURCE
    if (fmask & 0x00002000) info.setAudioCodec(parseField(tokens, index));        // fCODEC_AUDIO
    if (fmask & 0x00001000) info.setAudioBitrate(parseIntField(tokens, index));   // fBITRATE_AUDIO
    if (fmask & 0x00000800) info.setVideoCodec(parseField(tokens, index));        // fCODEC_VIDEO
    if (fmask & 0x00000400) info.setVideoBitrate(parseIntField(tokens, index));   // fBITRATE_VIDEO
    if (fmask & 0x00000200) info.setResolution(parseField(tokens, index));        // fRESOLUTION
    if (fmask & 0x00000100) info.setFileType(parseField(tokens, index));          // fFILETYPE
    if (fmask & 0x00000080) info.setAudioLanguagesFromString(parseField(tokens, index));  // fLANG_DUB
    if (fmask & 0x00000040) info.setSubtitleLanguagesFromString(parseField(tokens, index)); // fLANG_SUB
    if (fmask & 0x00000020) info.setLength(parseIntField(tokens, index));         // fLENGTH
    if (fmask & 0x00000010) info.setDescription(parseField(tokens, index));       // fDESCRIPTION
    if (fmask & 0x00000008) info.setAirDateFromUnix(parseInt64Field(tokens, index)); // fAIRDATE
    // Skip unused bits 0x00000004 and 0x00000002
    if (fmask & 0x00000001) info.setFilename(parseField(tokens, index));          // fFILENAME
    
    // FID is typically returned in the response code line, not in the data fields
    // It should be set by the caller after parsing
    
    return info;
}

void AniDBFileInfo::setEd2kHash(const QString& hash)
{
    // Validate ED2K hash format (32 hex characters)
    if (hash.isEmpty() || hash.length() == 32) {
        m_ed2k = hash.toLower();
    }
}

void AniDBFileInfo::setMd5Hash(const QString& hash)
{
    if (hash.isEmpty() || hash.length() == 32) {
        m_md5 = hash.toLower();
    }
}

void AniDBFileInfo::setSha1Hash(const QString& hash)
{
    if (hash.isEmpty() || hash.length() == 40) {
        m_sha1 = hash.toLower();
    }
}

void AniDBFileInfo::setAirDateFromUnix(qint64 timestamp)
{
    if (timestamp > 0) {
        m_airdate = QDateTime::fromSecsSinceEpoch(timestamp);
    }
}

void AniDBFileInfo::setAudioLanguagesFromString(const QString& langStr)
{
    if (!langStr.isEmpty()) {
        m_lang_dub = langStr.split('\'', Qt::SkipEmptyParts);
    }
}

void AniDBFileInfo::setSubtitleLanguagesFromString(const QString& langStr)
{
    if (!langStr.isEmpty()) {
        m_lang_sub = langStr.split('\'', Qt::SkipEmptyParts);
    }
}

QString AniDBFileInfo::formatSize() const
{
    double size = static_cast<double>(m_size);
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIndex]);
}

QString AniDBFileInfo::formatDuration() const
{
    if (m_length <= 0) {
        return QString();
    }
    
    int hours = m_length / 3600;
    int minutes = (m_length % 3600) / 60;
    int seconds = m_length % 60;
    
    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
            .arg(minutes)
            .arg(seconds, 2, 10, QChar('0'));
    }
}

int AniDBFileInfo::getVersion() const
{
    // Extract version from state flags
    if (m_state & STATE_ISV5) return 5;
    if (m_state & STATE_ISV4) return 4;
    if (m_state & STATE_ISV3) return 3;
    if (m_state & STATE_ISV2) return 2;
    return 1;  // Default version 1
}

AniDBFileInfo::LegacyFileData AniDBFileInfo::toLegacyStruct() const
{
    LegacyFileData data;
    data.fid = QString::number(m_fid);
    data.aid = QString::number(m_aid);
    data.eid = QString::number(m_eid);
    data.gid = QString::number(m_gid);
    data.lid = QString::number(m_lid);
    data.othereps = m_othereps;
    data.isdepr = m_isdepr ? "1" : "0";
    data.state = QString::number(m_state);
    data.size = QString::number(m_size);
    data.ed2k = m_ed2k;
    data.md5 = m_md5;
    data.sha1 = m_sha1;
    data.crc = m_crc;
    data.quality = m_quality;
    data.source = m_source;
    data.codec_audio = m_codec_audio;
    data.bitrate_audio = QString::number(m_bitrate_audio);
    data.codec_video = m_codec_video;
    data.bitrate_video = QString::number(m_bitrate_video);
    data.resolution = m_resolution;
    data.filetype = m_filetype;
    data.lang_dub = m_lang_dub.join("'");
    data.lang_sub = m_lang_sub.join("'");
    data.length = QString::number(m_length);
    data.description = m_description;
    data.airdate = QString::number(m_airdate.toSecsSinceEpoch());
    data.filename = m_filename;
    return data;
}

AniDBFileInfo AniDBFileInfo::fromLegacyStruct(const LegacyFileData& data)
{
    AniDBFileInfo info;
    info.setFileId(data.fid.toInt());
    info.setAnimeId(data.aid.toInt());
    info.setEpisodeId(data.eid.toInt());
    info.setGroupId(data.gid.toInt());
    info.setMylistId(data.lid.toInt());
    info.setOtherEpisodes(data.othereps);
    info.setDeprecated(data.isdepr == "1");
    info.setState(data.state.toInt());
    info.setSize(data.size.toLongLong());
    info.setEd2kHash(data.ed2k);
    info.setMd5Hash(data.md5);
    info.setSha1Hash(data.sha1);
    info.setCrc32(data.crc);
    info.setQuality(data.quality);
    info.setSource(data.source);
    info.setAudioCodec(data.codec_audio);
    info.setAudioBitrate(data.bitrate_audio.toInt());
    info.setVideoCodec(data.codec_video);
    info.setVideoBitrate(data.bitrate_video.toInt());
    info.setResolution(data.resolution);
    info.setFileType(data.filetype);
    info.setAudioLanguagesFromString(data.lang_dub);
    info.setSubtitleLanguagesFromString(data.lang_sub);
    info.setLength(data.length.toInt());
    info.setDescription(data.description);
    info.setAirDateFromUnix(data.airdate.toLongLong());
    info.setFilename(data.filename);
    return info;
}

// Static helper methods for parsing

QString AniDBFileInfo::parseField(const QStringList& tokens, int& index)
{
    if (index < tokens.size()) {
        return tokens[index++];
    }
    return QString();
}

int AniDBFileInfo::parseIntField(const QStringList& tokens, int& index)
{
    if (index < tokens.size()) {
        bool ok;
        int value = tokens[index++].toInt(&ok);
        return ok ? value : 0;
    }
    return 0;
}

qint64 AniDBFileInfo::parseInt64Field(const QStringList& tokens, int& index)
{
    if (index < tokens.size()) {
        bool ok;
        qint64 value = tokens[index++].toLongLong(&ok);
        return ok ? value : 0;
    }
    return 0;
}

bool AniDBFileInfo::parseBoolField(const QStringList& tokens, int& index)
{
    if (index < tokens.size()) {
        QString value = tokens[index++];
        return (value == "1" || value.toLower() == "true");
    }
    return false;
}

QDateTime AniDBFileInfo::parseDateField(const QStringList& tokens, int& index)
{
    if (index < tokens.size()) {
        qint64 timestamp = tokens[index++].toLongLong();
        if (timestamp > 0) {
            return QDateTime::fromSecsSinceEpoch(timestamp);
        }
    }
    return QDateTime();
}
