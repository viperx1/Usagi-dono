#include "anidbanimeinfo.h"

AniDBAnimeInfo::AniDBAnimeInfo()
    : m_aid(0)
    , m_episodes(0)
    , m_special_ep_count(0)
    , m_specials_count(0)
    , m_credits_count(0)
    , m_other_count(0)
    , m_trailer_count(0)
    , m_parody_count(0)
    , m_vote_count(0)
    , m_temp_vote_count(0)
    , m_review_count(0)
    , m_is_18_restricted(false)
    , m_ann_id(0)
    , m_allcinema_id(0)
    , m_date_record_updated(0)
{
}

void AniDBAnimeInfo::setAirDate(const QString& date)
{
    // Store date as-is (should be in ISO format YYYY-MM-DDZ)
    m_air_date = date;
}

void AniDBAnimeInfo::setEndDate(const QString& date)
{
    // Store date as-is (should be in ISO format YYYY-MM-DDZ)
    m_end_date = date;
}

AniDBAnimeInfo::LegacyAnimeData AniDBAnimeInfo::toLegacyStruct() const
{
    LegacyAnimeData data;
    
    data.aid = QString::number(m_aid);
    data.dateflags = m_dateflags;
    data.year = m_year;
    data.type = m_type;
    data.relaidlist = m_relaidlist;
    data.relaidtype = m_relaidtype;
    
    data.nameromaji = m_nameromaji;
    data.namekanji = m_namekanji;
    data.nameenglish = m_nameenglish;
    data.nameother = m_nameother;
    data.nameshort = m_nameshort;
    data.synonyms = m_synonyms;
    
    data.episodes = QString::number(m_episodes);
    data.highest_episode = m_highest_episode;
    data.special_ep_count = QString::number(m_special_ep_count);
    data.air_date = m_air_date;
    data.end_date = m_end_date;
    data.url = m_url;
    data.picname = m_picname;
    
    data.rating = m_rating;
    data.vote_count = QString::number(m_vote_count);
    data.temp_rating = m_temp_rating;
    data.temp_vote_count = QString::number(m_temp_vote_count);
    data.avg_review_rating = m_avg_review_rating;
    data.review_count = QString::number(m_review_count);
    data.award_list = m_award_list;
    data.is_18_restricted = m_is_18_restricted ? "1" : "0";
    
    data.ann_id = QString::number(m_ann_id);
    data.allcinema_id = QString::number(m_allcinema_id);
    data.animenfo_id = m_animenfo_id;
    data.tag_name_list = m_tag_name_list;
    data.tag_id_list = m_tag_id_list;
    data.tag_weight_list = m_tag_weight_list;
    data.date_record_updated = QString::number(m_date_record_updated);
    
    data.character_id_list = m_character_id_list;
    data.specials_count = QString::number(m_specials_count);
    data.credits_count = QString::number(m_credits_count);
    data.other_count = QString::number(m_other_count);
    data.trailer_count = QString::number(m_trailer_count);
    data.parody_count = QString::number(m_parody_count);
    
    data.eptotal = m_eptotal;
    data.eplast = m_eplast;
    data.category = m_category;
    
    return data;
}

AniDBAnimeInfo AniDBAnimeInfo::fromLegacyStruct(const LegacyAnimeData& data)
{
    AniDBAnimeInfo info;
    
    info.setAnimeId(data.aid.toInt());
    info.setDateFlags(data.dateflags);
    info.setYear(data.year);
    info.setType(data.type);
    info.setRelatedAnimeIds(data.relaidlist);
    info.setRelatedAnimeTypes(data.relaidtype);
    
    info.setNameRomaji(data.nameromaji);
    info.setNameKanji(data.namekanji);
    info.setNameEnglish(data.nameenglish);
    info.setNameOther(data.nameother);
    info.setNameShort(data.nameshort);
    info.setSynonyms(data.synonyms);
    
    info.setEpisodeCount(data.episodes.toInt());
    info.setHighestEpisode(data.highest_episode);
    info.setSpecialEpisodeCount(data.special_ep_count.toInt());
    info.setAirDate(data.air_date);
    info.setEndDate(data.end_date);
    info.setUrl(data.url);
    info.setPictureName(data.picname);
    
    info.setRating(data.rating);
    info.setVoteCount(data.vote_count.toInt());
    info.setTempRating(data.temp_rating);
    info.setTempVoteCount(data.temp_vote_count.toInt());
    info.setAvgReviewRating(data.avg_review_rating);
    info.setReviewCount(data.review_count.toInt());
    info.setAwardList(data.award_list);
    info.set18Restricted(data.is_18_restricted == "1");
    
    info.setAnnId(data.ann_id.toInt());
    info.setAllCinemaId(data.allcinema_id.toInt());
    info.setAnimeNfoId(data.animenfo_id);
    info.setTagNameList(data.tag_name_list);
    info.setTagIdList(data.tag_id_list);
    info.setTagWeightList(data.tag_weight_list);
    info.setDateRecordUpdated(data.date_record_updated.toLongLong());
    
    info.setCharacterIdList(data.character_id_list);
    info.setSpecialsCount(data.specials_count.toInt());
    info.setCreditsCount(data.credits_count.toInt());
    info.setOtherCount(data.other_count.toInt());
    info.setTrailerCount(data.trailer_count.toInt());
    info.setParodyCount(data.parody_count.toInt());
    
    info.setEptotal(data.eptotal);
    info.setEplast(data.eplast);
    info.setCategory(data.category);
    
    return info;
}
