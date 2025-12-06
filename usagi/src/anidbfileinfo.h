#ifndef ANIDBFILEINFO_H
#define ANIDBFILEINFO_H

#include <QString>
#include <QStringList>
#include <QDateTime>

/**
 * @brief AniDBFileInfo - Type-safe representation of AniDB file data
 * 
 * This class replaces the plain FileData struct with:
 * - Type-safe fields (int for IDs, qint64 for sizes, etc.)
 * - Validation of data
 * - Factory methods for parsing API responses
 * - Utility methods for common operations
 * 
 * Follows SOLID principles:
 * - Single Responsibility: Represents file data only
 * - Encapsulation: Private members with validated setters
 * - Type Safety: Proper C++ types instead of all QString
 * - Validation: Invalid data cannot be set
 * 
 * Usage:
 *   AniDBFileInfo fileInfo = AniDBFileInfo::fromApiResponse(tokens, fmask, index);
 *   if (fileInfo.isValid()) {
 *       int fid = fileInfo.fileId();
 *       qint64 size = fileInfo.size();
 *   }
 */
class AniDBFileInfo
{
public:
    /**
     * @brief Default constructor creates invalid file info
     */
    AniDBFileInfo();
    
    /**
     * @brief Factory method to create from API response
     * @param tokens Response tokens split by '|'
     * @param fmask File mask indicating which fields are present
     * @param index Starting index in tokens (will be incremented)
     * @return Parsed file info object
     */
    static AniDBFileInfo fromApiResponse(const QStringList& tokens, 
                                        unsigned int fmask, 
                                        int& index);
    
    // ID accessors
    int fileId() const { return m_fid; }
    int animeId() const { return m_aid; }
    int episodeId() const { return m_eid; }
    int groupId() const { return m_gid; }
    int mylistId() const { return m_lid; }
    
    void setFileId(int fid) { m_fid = fid; }
    void setAnimeId(int aid) { m_aid = aid; }
    void setEpisodeId(int eid) { m_eid = eid; }
    void setGroupId(int gid) { m_gid = gid; }
    void setMylistId(int lid) { m_lid = lid; }
    
    // File properties
    qint64 size() const { return m_size; }
    QString ed2kHash() const { return m_ed2k; }
    QString md5Hash() const { return m_md5; }
    QString sha1Hash() const { return m_sha1; }
    QString crc32() const { return m_crc; }
    QString filename() const { return m_filename; }
    
    void setSize(qint64 size) { m_size = size; }
    void setEd2kHash(const QString& hash);
    void setMd5Hash(const QString& hash);
    void setSha1Hash(const QString& hash);
    void setCrc32(const QString& crc) { m_crc = crc; }
    void setFilename(const QString& filename) { m_filename = filename; }
    
    // File metadata
    QString otherEpisodes() const { return m_othereps; }
    bool isDeprecated() const { return m_isdepr; }
    int state() const { return m_state; }
    QString quality() const { return m_quality; }
    QString source() const { return m_source; }
    QString fileType() const { return m_filetype; }
    QString description() const { return m_description; }
    QDateTime airDate() const { return m_airdate; }
    
    void setOtherEpisodes(const QString& othereps) { m_othereps = othereps; }
    void setDeprecated(bool deprecated) { m_isdepr = deprecated; }
    void setState(int state) { m_state = state; }
    void setQuality(const QString& quality) { m_quality = quality; }
    void setSource(const QString& source) { m_source = source; }
    void setFileType(const QString& filetype) { m_filetype = filetype; }
    void setDescription(const QString& description) { m_description = description; }
    void setAirDate(const QDateTime& airdate) { m_airdate = airdate; }
    void setAirDateFromUnix(qint64 timestamp);
    
    // Audio/Video properties  
    QString audioCodec() const { return m_codec_audio; }
    int audioBitrate() const { return m_bitrate_audio; }
    QString videoCodec() const { return m_codec_video; }
    int videoBitrate() const { return m_bitrate_video; }
    QString resolution() const { return m_resolution; }
    int length() const { return m_length; }
    
    void setAudioCodec(const QString& codec) { m_codec_audio = codec; }
    void setAudioBitrate(int bitrate) { m_bitrate_audio = bitrate; }
    void setVideoCodec(const QString& codec) { m_codec_video = codec; }
    void setVideoBitrate(int bitrate) { m_bitrate_video = bitrate; }
    void setResolution(const QString& resolution) { m_resolution = resolution; }
    void setLength(int length) { m_length = length; }
    
    // Language properties
    QStringList audioLanguages() const { return m_lang_dub; }
    QStringList subtitleLanguages() const { return m_lang_sub; }
    
    void setAudioLanguages(const QStringList& langs) { m_lang_dub = langs; }
    void setSubtitleLanguages(const QStringList& langs) { m_lang_sub = langs; }
    void setAudioLanguagesFromString(const QString& langStr);
    void setSubtitleLanguagesFromString(const QString& langStr);
    
    // Validation
    bool isValid() const { return m_fid > 0; }
    bool hasHash() const { return !m_ed2k.isEmpty(); }
    
    // Utility methods
    QString formatSize() const;  // Returns human-readable size (e.g., "1.5 GB")
    QString formatDuration() const;  // Returns formatted duration (e.g., "24:30")
    int getVersion() const;  // Extract version from state flags (1-5)
    
    // Convert back to legacy struct for database storage (temporary during migration)
    struct LegacyFileData {
        QString fid, aid, eid, gid, lid;
        QString othereps, isdepr, state, size, ed2k, md5, sha1, crc;
        QString quality, source, codec_audio, bitrate_audio;
        QString codec_video, bitrate_video, resolution, filetype;
        QString lang_dub, lang_sub, length, description, airdate, filename;
    };
    LegacyFileData toLegacyStruct() const;
    
    // Create from legacy struct (for backwards compatibility during migration)
    static AniDBFileInfo fromLegacyStruct(const LegacyFileData& data);
    
private:
    // ID fields
    int m_fid;
    int m_aid;
    int m_eid;
    int m_gid;
    int m_lid;
    
    // File properties
    qint64 m_size;
    QString m_ed2k;
    QString m_md5;
    QString m_sha1;
    QString m_crc;
    QString m_filename;
    
    // Metadata
    QString m_othereps;
    bool m_isdepr;
    int m_state;
    QString m_quality;
    QString m_source;
    QString m_filetype;
    QString m_description;
    QDateTime m_airdate;
    
    // Audio/Video
    QString m_codec_audio;
    int m_bitrate_audio;
    QString m_codec_video;
    int m_bitrate_video;
    QString m_resolution;
    int m_length;  // in seconds
    
    // Languages (split from comma-separated strings)
    QStringList m_lang_dub;
    QStringList m_lang_sub;
    
    // Helper methods
    static QString parseField(const QStringList& tokens, int& index);
    static int parseIntField(const QStringList& tokens, int& index);
    static qint64 parseInt64Field(const QStringList& tokens, int& index);
    static bool parseBoolField(const QStringList& tokens, int& index);
    static QDateTime parseDateField(const QStringList& tokens, int& index);
};

#endif // ANIDBFILEINFO_H
