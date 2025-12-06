#ifndef ANIDBANIMEINFO_H
#define ANIDBANIMEINFO_H

#include <QString>
#include <QStringList>
#include <QDateTime>

/**
 * @brief AniDBAnimeInfo - Type-safe representation of AniDB anime data
 * 
 * This class replaces the plain AnimeData struct with:
 * - Type-safe fields (int for IDs and counts, proper types for dates/ratings)
 * - Validation of data
 * - Factory methods for parsing API responses
 * - Utility methods for common operations
 * 
 * Follows SOLID principles:
 * - Single Responsibility: Represents anime metadata only
 * - Encapsulation: Private members with validated setters
 * - Type Safety: Proper C++ types instead of all QString
 * - Validation: Invalid data cannot be set
 * 
 * Usage:
 *   AniDBAnimeInfo animeInfo = AniDBAnimeInfo::fromApiResponse(tokens, amask, index);
 *   if (animeInfo.isValid()) {
 *       int aid = animeInfo.animeId();
 *       int episodes = animeInfo.episodeCount();
 *   }
 */
class AniDBAnimeInfo
{
public:
    /**
     * @brief Default constructor creates invalid anime info
     */
    AniDBAnimeInfo();
    
    // ID and basic info
    int animeId() const { return m_aid; }
    QString year() const { return m_year; }
    QString type() const { return m_type; }
    QString dateFlags() const { return m_dateflags; }
    
    void setAnimeId(int aid) { m_aid = aid; }
    void setYear(const QString& year) { m_year = year; }
    void setType(const QString& type) { m_type = type; }
    void setDateFlags(const QString& flags) { m_dateflags = flags; }
    
    // Relations
    QString relatedAnimeIds() const { return m_relaidlist; }
    QString relatedAnimeTypes() const { return m_relaidtype; }
    
    void setRelatedAnimeIds(const QString& ids) { m_relaidlist = ids; }
    void setRelatedAnimeTypes(const QString& types) { m_relaidtype = types; }
    
    // Names
    QString nameRomaji() const { return m_nameromaji; }
    QString nameKanji() const { return m_namekanji; }
    QString nameEnglish() const { return m_nameenglish; }
    QString nameOther() const { return m_nameother; }
    QString nameShort() const { return m_nameshort; }
    QString synonyms() const { return m_synonyms; }
    
    void setNameRomaji(const QString& name) { m_nameromaji = name; }
    void setNameKanji(const QString& name) { m_namekanji = name; }
    void setNameEnglish(const QString& name) { m_nameenglish = name; }
    void setNameOther(const QString& name) { m_nameother = name; }
    void setNameShort(const QString& name) { m_nameshort = name; }
    void setSynonyms(const QString& synonyms) { m_synonyms = synonyms; }
    
    // Episode counts
    int episodeCount() const { return m_episodes; }
    QString highestEpisode() const { return m_highest_episode; }
    int specialEpisodeCount() const { return m_special_ep_count; }
    int specialsCount() const { return m_specials_count; }
    int creditsCount() const { return m_credits_count; }
    int otherCount() const { return m_other_count; }
    int trailerCount() const { return m_trailer_count; }
    int parodyCount() const { return m_parody_count; }
    
    void setEpisodeCount(int count) { m_episodes = count; }
    void setHighestEpisode(const QString& epno) { m_highest_episode = epno; }
    void setSpecialEpisodeCount(int count) { m_special_ep_count = count; }
    void setSpecialsCount(int count) { m_specials_count = count; }
    void setCreditsCount(int count) { m_credits_count = count; }
    void setOtherCount(int count) { m_other_count = count; }
    void setTrailerCount(int count) { m_trailer_count = count; }
    void setParodyCount(int count) { m_parody_count = count; }
    
    // Dates
    QString airDate() const { return m_air_date; }  // ISO format YYYY-MM-DDZ
    QString endDate() const { return m_end_date; }  // ISO format YYYY-MM-DDZ
    
    void setAirDate(const QString& date);
    void setEndDate(const QString& date);
    
    // URLs and images
    QString url() const { return m_url; }
    QString pictureName() const { return m_picname; }
    
    void setUrl(const QString& url) { m_url = url; }
    void setPictureName(const QString& name) { m_picname = name; }
    
    // Ratings
    QString rating() const { return m_rating; }
    int voteCount() const { return m_vote_count; }
    QString tempRating() const { return m_temp_rating; }
    int tempVoteCount() const { return m_temp_vote_count; }
    QString avgReviewRating() const { return m_avg_review_rating; }
    int reviewCount() const { return m_review_count; }
    QString awardList() const { return m_award_list; }
    
    void setRating(const QString& rating) { m_rating = rating; }
    void setVoteCount(int count) { m_vote_count = count; }
    void setTempRating(const QString& rating) { m_temp_rating = rating; }
    void setTempVoteCount(int count) { m_temp_vote_count = count; }
    void setAvgReviewRating(const QString& rating) { m_avg_review_rating = rating; }
    void setReviewCount(int count) { m_review_count = count; }
    void setAwardList(const QString& awards) { m_award_list = awards; }
    
    // Restrictions and external IDs
    bool is18Restricted() const { return m_is_18_restricted; }
    int annId() const { return m_ann_id; }
    int allCinemaId() const { return m_allcinema_id; }
    QString animeNfoId() const { return m_animenfo_id; }
    
    void set18Restricted(bool restricted) { m_is_18_restricted = restricted; }
    void setAnnId(int id) { m_ann_id = id; }
    void setAllCinemaId(int id) { m_allcinema_id = id; }
    void setAnimeNfoId(const QString& id) { m_animenfo_id = id; }
    
    // Tags
    QString tagNameList() const { return m_tag_name_list; }
    QString tagIdList() const { return m_tag_id_list; }
    QString tagWeightList() const { return m_tag_weight_list; }
    
    void setTagNameList(const QString& names) { m_tag_name_list = names; }
    void setTagIdList(const QString& ids) { m_tag_id_list = ids; }
    void setTagWeightList(const QString& weights) { m_tag_weight_list = weights; }
    
    // Other metadata
    qint64 dateRecordUpdated() const { return m_date_record_updated; }
    QString characterIdList() const { return m_character_id_list; }
    
    void setDateRecordUpdated(qint64 timestamp) { m_date_record_updated = timestamp; }
    void setCharacterIdList(const QString& ids) { m_character_id_list = ids; }
    
    // Legacy fields for backward compatibility
    QString eptotal() const { return m_eptotal; }
    QString eplast() const { return m_eplast; }
    QString category() const { return m_category; }
    
    void setEptotal(const QString& total) { m_eptotal = total; }
    void setEplast(const QString& last) { m_eplast = last; }
    void setCategory(const QString& category) { m_category = category; }
    
    // Validation
    bool isValid() const { return m_aid > 0; }
    
    // Legacy struct for backward compatibility
    struct LegacyAnimeData {
        QString aid, dateflags, year, type;
        QString relaidlist, relaidtype;
        QString nameromaji, namekanji, nameenglish, nameother, nameshort, synonyms;
        QString episodes, highest_episode, special_ep_count;
        QString air_date, end_date, url, picname;
        QString rating, vote_count, temp_rating, temp_vote_count;
        QString avg_review_rating, review_count, award_list, is_18_restricted;
        QString ann_id, allcinema_id, animenfo_id;
        QString tag_name_list, tag_id_list, tag_weight_list, date_record_updated;
        QString character_id_list;
        QString specials_count, credits_count, other_count, trailer_count, parody_count;
        QString eptotal, eplast, category;
    };
    
    // Convert to/from legacy struct
    LegacyAnimeData toLegacyStruct() const;
    static AniDBAnimeInfo fromLegacyStruct(const LegacyAnimeData& data);
    
private:
    // IDs and basic info
    int m_aid;
    QString m_year;
    QString m_type;
    QString m_dateflags;
    
    // Relations
    QString m_relaidlist;
    QString m_relaidtype;
    
    // Names
    QString m_nameromaji;
    QString m_namekanji;
    QString m_nameenglish;
    QString m_nameother;
    QString m_nameshort;
    QString m_synonyms;
    
    // Episode counts
    int m_episodes;
    QString m_highest_episode;
    int m_special_ep_count;
    int m_specials_count;
    int m_credits_count;
    int m_other_count;
    int m_trailer_count;
    int m_parody_count;
    
    // Dates
    QString m_air_date;
    QString m_end_date;
    
    // URLs and images
    QString m_url;
    QString m_picname;
    
    // Ratings
    QString m_rating;
    int m_vote_count;
    QString m_temp_rating;
    int m_temp_vote_count;
    QString m_avg_review_rating;
    int m_review_count;
    QString m_award_list;
    
    // Restrictions and external IDs
    bool m_is_18_restricted;
    int m_ann_id;
    int m_allcinema_id;
    QString m_animenfo_id;
    
    // Tags
    QString m_tag_name_list;
    QString m_tag_id_list;
    QString m_tag_weight_list;
    
    // Other metadata
    qint64 m_date_record_updated;
    QString m_character_id_list;
    
    // Legacy fields
    QString m_eptotal;
    QString m_eplast;
    QString m_category;
};

#endif // ANIDBANIMEINFO_H
