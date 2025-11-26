#include "mylistcardmanager.h"
#include "animeutils.h"
#include "logger.h"
#include "main.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QElapsedTimer>
#include <QDateTime>
#include <QNetworkRequest>
#include <algorithm>
#include <numeric>

// External references
extern myAniDBApi *adbapi;

MyListCardManager::MyListCardManager(QObject *parent)
    : QObject(parent)
    , m_layout(nullptr)
    , m_networkManager(nullptr)
{
    // Initialize network manager for poster downloads
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &MyListCardManager::onPosterDownloadFinished);
    
    // Initialize batch update timer
    m_batchUpdateTimer = new QTimer(this);
    m_batchUpdateTimer->setSingleShot(true);
    m_batchUpdateTimer->setInterval(BATCH_UPDATE_DELAY);
    connect(m_batchUpdateTimer, &QTimer::timeout,
            this, &MyListCardManager::processBatchedUpdates);
}

MyListCardManager::~MyListCardManager()
{
    clearAllCards();
}

void MyListCardManager::setCardLayout(FlowLayout *layout)
{
    QMutexLocker locker(&m_mutex);
    m_layout = layout;
}

void MyListCardManager::loadAllCards()
{
    QElapsedTimer timer;
    timer.start();
    
    LOG(QString("[MyListCardManager] Starting loadAllCards"));
    
    // Clear existing cards if any
    clearAllCards();
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    // Query all anime in mylist
    QString query = "SELECT m.aid, "
                    "a.nameromaji, a.nameenglish, a.eptotal, "
                    "at.title as anime_title, "
                    "a.eps, a.typename, a.startdate, a.enddate, a.picname, a.poster_image, a.category, "
                    "a.rating, a.tag_name_list, a.tag_id_list, a.tag_weight_list, a.is_18_restricted "
                    "FROM mylist m "
                    "LEFT JOIN anime a ON m.aid = a.aid "
                    "LEFT JOIN anime_titles at ON m.aid = at.aid AND at.type = 1 "
                    "GROUP BY m.aid "
                    "ORDER BY a.nameromaji";
    
    QSqlQuery q(db);
    
    if (!q.exec(query)) {
        LOG("[MyListCardManager] Error loading mylist: " + q.lastError().text());
        return;
    }
    
    // Collect all AIDs and preload anime titles for those without anime table data
    QList<int> aids;
    QList<int> aidsNeedingTitles;
    while (q.next()) {
        int aid = q.value(0).toInt();
        aids.append(aid);
        
        // Check if this anime has data in the anime table (nameromaji is a good indicator)
        if (q.value(1).isNull() || q.value(1).toString().isEmpty()) {
            aidsNeedingTitles.append(aid);
        }
    }
    
    // Preload titles for anime without anime table data
    if (!aidsNeedingTitles.isEmpty()) {
        QString aidsList = QStringList(
            std::accumulate(aidsNeedingTitles.begin(), aidsNeedingTitles.end(), QStringList(),
                [](QStringList acc, int aid) { acc.append(QString::number(aid)); return acc; })
        ).join(",");
        
        QString titlesQuery = QString("SELECT aid, title FROM anime_titles "
                                     "WHERE aid IN (%1) AND type = 1 AND language = 'x-jat'").arg(aidsList);
        QSqlQuery tq(db);
        if (tq.exec(titlesQuery)) {
            while (tq.next()) {
                int aid = tq.value(0).toInt();
                QString title = tq.value(1).toString();
                m_animeTitlesCache[aid] = title;
            }
            LOG(QString("[MyListCardManager] Preloaded %1 anime titles into cache for mylist").arg(m_animeTitlesCache.size()));
        }
    }
    
    // Bulk preload anime data and statistics for better performance
    preloadAnimeDataCache(aids);
    
    // Bulk preload episode data to eliminate individual queries during card creation
    preloadEpisodesCache(aids);
    
    int cardCount = 0;
    
    // Create cards for each anime
    for (int aid : aids) {
        // Skip if card already exists
        if (hasCard(aid)) {
            continue;
        }
        
        // Create the card (this will use the preloaded cache if needed)
        AnimeCard *card = createCard(aid);
        if (card) {
            cardCount++;
        }
    }
    
    qint64 elapsed = timer.elapsed();
    LOG(QString("[MyListCardManager] Loaded %1 cards in %2 ms").arg(cardCount).arg(elapsed));
    
    emit allCardsLoaded(cardCount);
}

void MyListCardManager::loadAllAnimeTitles()
{
    QElapsedTimer timer;
    timer.start();
    
    LOG(QString("[MyListCardManager] Starting loadAllAnimeTitles - loading all anime from anime_titles"));
    
    // Clear existing cards if any
    clearAllCards();
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    // Query all anime from anime_titles table
    // Prefer type 1 (main title) and language "x-jat" (Japanese transcription/romaji)
    QString query = "SELECT DISTINCT at.aid, at.title "
                    "FROM anime_titles at "
                    "WHERE at.type = 1 AND at.language = 'x-jat' "
                    "GROUP BY at.aid "
                    "ORDER BY at.title";
    
    QSqlQuery q(db);
    
    if (!q.exec(query)) {
        LOG("[MyListCardManager] Error loading anime_titles: " + q.lastError().text());
        return;
    }
    
    // Preload all anime titles into cache for bulk access
    QList<int> aids;
    m_animeTitlesCache.clear();
    while (q.next()) {
        int aid = q.value(0).toInt();
        QString title = q.value(1).toString();
        
        // Only add if not already in cache (prevents duplicates)
        if (!m_animeTitlesCache.contains(aid)) {
            aids.append(aid);
            m_animeTitlesCache[aid] = title;
        }
    }
    
    LOG(QString("[MyListCardManager] Preloaded %1 anime titles into cache").arg(m_animeTitlesCache.size()));
    
    // Bulk preload anime data and statistics (much faster than individual queries)
    qint64 preloadStart = timer.elapsed();
    preloadAnimeDataCache(aids);
    qint64 preloadElapsed = timer.elapsed() - preloadStart;
    LOG(QString("[MyListCardManager] Bulk anime data preload took %1 ms").arg(preloadElapsed));
    
    // Bulk preload episode data to eliminate individual queries during card creation
    qint64 episodesPreloadStart = timer.elapsed();
    preloadEpisodesCache(aids);
    qint64 episodesPreloadElapsed = timer.elapsed() - episodesPreloadStart;
    LOG(QString("[MyListCardManager] Bulk episodes preload took %1 ms").arg(episodesPreloadElapsed));
    
    int cardCount = 0;
    
    // Disable layout updates during bulk card creation for performance
    if (m_layout && m_layout->parent()) {
        QWidget *container = qobject_cast<QWidget*>(m_layout->parent());
        if (container) {
            container->setUpdatesEnabled(false);
        }
    }
    
    // Create cards for each anime
    for (int aid : aids) {
        // Skip if card already exists
        if (hasCard(aid)) {
            continue;
        }
        
        // Create the card (this will use the preloaded caches)
        AnimeCard *card = createCard(aid);
        if (card) {
            cardCount++;
        }
    }
    
    // Re-enable layout updates and force a single refresh
    if (m_layout && m_layout->parent()) {
        QWidget *container = qobject_cast<QWidget*>(m_layout->parent());
        if (container) {
            container->setUpdatesEnabled(true);
            container->update();
        }
    }
    
    qint64 elapsed = timer.elapsed();
    LOG(QString("[MyListCardManager] Loaded %1 anime title cards in %2 ms").arg(cardCount).arg(elapsed));
    
    emit allCardsLoaded(cardCount);
}

void MyListCardManager::clearAllCards()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_layout) {
        // Remove cards from layout and delete them
        const auto cardValues = m_cards.values();
        for (AnimeCard* const card : cardValues) {
            m_layout->removeWidget(card);
            delete card;
        }
    }
    
    m_cards.clear();
    m_animeTitlesCache.clear();  // Clear the titles cache
    m_animeDataCache.clear();  // Clear the anime data cache
    m_statsCache.clear();  // Clear the statistics cache
    m_episodesCache.clear();  // Clear the episodes cache
    m_episodesNeedingData.clear();
    m_animeNeedingMetadata.clear();
    m_animeNeedingPoster.clear();
    m_animePicnames.clear();
    // Note: NOT clearing m_animeMetadataRequested to prevent re-requesting
}

AnimeCard* MyListCardManager::getCard(int aid)
{
    QMutexLocker locker(&m_mutex);
    return m_cards.value(aid, nullptr);
}

bool MyListCardManager::hasCard(int aid) const
{
    QMutexLocker locker(&m_mutex);
    return m_cards.contains(aid);
}

QList<AnimeCard*> MyListCardManager::getAllCards() const
{
    QMutexLocker locker(&m_mutex);
    return m_cards.values();
}

void MyListCardManager::updateCardAnimeInfo(int aid)
{
    QMutexLocker locker(&m_mutex);
    
    // Add to pending updates for batch processing
    m_pendingCardUpdates.insert(aid);
    
    // Start timer if not already running
    if (!m_batchUpdateTimer->isActive()) {
        m_batchUpdateTimer->start();
    }
}

void MyListCardManager::updateCardEpisode(int aid, int eid)
{
    // Defer to full card update for now
    // Could be optimized to only update specific episode in the future
    updateCardAnimeInfo(aid);
}

void MyListCardManager::updateCardStatistics(int aid)
{
    updateCardAnimeInfo(aid);
}

void MyListCardManager::updateCardPoster(int aid, const QString &picname)
{
    if (picname.isEmpty()) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    m_animePicnames[aid] = picname;
    m_animeNeedingPoster.insert(aid);
    
    // Release lock before calling download
    locker.unlock();
    
    downloadPoster(aid, picname);
}

void MyListCardManager::updateMultipleCards(const QSet<int> &aids)
{
    QMutexLocker locker(&m_mutex);
    
    m_pendingCardUpdates.unite(aids);
    
    if (!m_batchUpdateTimer->isActive()) {
        m_batchUpdateTimer->start();
    }
}

void MyListCardManager::updateOrAddMylistEntry(int lid)
{
    // Query the database for this mylist entry
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT aid FROM mylist WHERE lid = ?");
    q.addBindValue(lid);
    
    if (!q.exec() || !q.next()) {
        LOG(QString("[MyListCardManager] Error querying mylist entry lid=%1: %2")
            .arg(lid).arg(q.lastError().text()));
        return;
    }
    
    int aid = q.value(0).toInt();
    
    // Check if card exists
    QMutexLocker locker(&m_mutex);
    if (!m_cards.contains(aid)) {
        // Card doesn't exist, create it
        locker.unlock();
        AnimeCard *card = createCard(aid);
        if (!card) {
            LOG(QString("[MyListCardManager] Failed to create card for aid=%1").arg(aid));
        }
    } else {
        // Card exists, update it
        locker.unlock();
        updateCardAnimeInfo(aid);
    }
}

void MyListCardManager::onEpisodeUpdated(int eid, int aid)
{
    // Schedule card update
    updateCardEpisode(aid, eid);
    
    // Remove from tracking
    QMutexLocker locker(&m_mutex);
    m_episodesNeedingData.remove(eid);
}

void MyListCardManager::onAnimeUpdated(int aid)
{
    // Schedule card update
    updateCardAnimeInfo(aid);
    
    // Remove from tracking
    QMutexLocker locker(&m_mutex);
    m_animeNeedingMetadata.remove(aid);
    AnimeCard *card = m_cards.value(aid, nullptr);
    
    // Hide warning only if both metadata and poster are no longer needed
    bool stillNeedsData = m_animeNeedingPoster.contains(aid);
    locker.unlock();
    
    if (card && !stillNeedsData) {
        card->setNeedsFetch(false);
    }
}

void MyListCardManager::onFetchDataRequested(int aid)
{
    LOG(QString("[MyListCardManager] Fetch data requested for anime %1").arg(aid));
    
    QMutexLocker locker(&m_mutex);
    
    // Check what data is missing
    bool needsMetadata = m_animeNeedingMetadata.contains(aid);
    bool needsPoster = m_animeNeedingPoster.contains(aid);
    bool hasEpisodesNeedingData = false;
    QSet<int> episodesNeedingData;
    
    // Check if any episodes need data
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        QSqlQuery q(db);
        // Check for episodes that either:
        // 1. Don't exist in episode table at all (e.eid IS NULL)
        // 2. Exist but have missing/empty name or epno fields
        q.prepare("SELECT DISTINCT m.eid FROM mylist m "
                 "LEFT JOIN episode e ON m.eid = e.eid "
                 "WHERE m.aid = ? AND m.eid > 0 AND (e.eid IS NULL OR e.name IS NULL OR e.name = '' OR e.epno IS NULL OR e.epno = '')");
        q.addBindValue(aid);
        if (q.exec()) {
            LOG(QString("[MyListCardManager] Checking episodes for aid=%1").arg(aid));
            while (q.next()) {
                int eid = q.value(0).toInt();
                LOG(QString("[MyListCardManager]   Found episode needing data: eid=%1").arg(eid));
                if (eid > 0) {
                    hasEpisodesNeedingData = true;
                    episodesNeedingData.insert(eid);
                }
            }
        } else {
            LOG(QString("[MyListCardManager] Failed to query episodes needing data: %1").arg(q.lastError().text()));
        }
    }
    
    LOG(QString("[MyListCardManager] Data check for aid=%1: needsMetadata=%2, needsPoster=%3, hasEpisodesNeedingData=%4 (count=%5), alreadyRequested=%6")
        .arg(aid).arg(needsMetadata).arg(needsPoster).arg(hasEpisodesNeedingData).arg(episodesNeedingData.size()).arg(m_animeMetadataRequested.contains(aid)));
    
    bool requestedAnything = false;
    
    // Request metadata if needed and not already requested
    if (!m_animeMetadataRequested.contains(aid)) {
        m_animeMetadataRequested.insert(aid);
        needsMetadata = true;
        requestedAnything = true;
        LOG(QString("[MyListCardManager] Will request anime metadata for aid=%1").arg(aid));
    }
    
    // Request poster if needed and not already downloaded
    if (m_animeNeedingPoster.contains(aid) && m_animePicnames.contains(aid)) {
        QString picname = m_animePicnames[aid];
        needsPoster = true;
        requestedAnything = true;
        LOG(QString("[MyListCardManager] Will download poster for aid=%1, picname=%2").arg(aid).arg(picname));
        locker.unlock();
        
        if (needsMetadata) {
            requestAnimeMetadata(aid);
        }
        if (needsPoster) {
            downloadPoster(aid, picname);
        }
    } else {
        locker.unlock();
        if (needsMetadata) {
            requestAnimeMetadata(aid);
        }
    }
    
    // Request episode data for episodes that need it
    if (hasEpisodesNeedingData) {
        LOG(QString("[MyListCardManager] Requesting episode data for %1 episodes of aid=%2").arg(episodesNeedingData.size()).arg(aid));
        for (int eid : episodesNeedingData) {
            LOG(QString("[MyListCardManager] Emitting episodeDataRequested signal for eid=%1").arg(eid));
            emit episodeDataRequested(eid);
        }
        requestedAnything = true;
    }
    
    if (!requestedAnything) {
        LOG(QString("[MyListCardManager] No data needs to be fetched for aid=%1 (already complete or requested)").arg(aid));
    }
}

void MyListCardManager::onPosterDownloadFinished(QNetworkReply *reply)
{
    // Get the anime ID for this download
    QMutexLocker locker(&m_mutex);
    
    if (!m_posterDownloadRequests.contains(reply)) {
        reply->deleteLater();
        return;
    }
    
    int aid = m_posterDownloadRequests.take(reply);
    AnimeCard *card = m_cards.value(aid, nullptr);
    
    locker.unlock();
    
    if (!card) {
        LOG(QString("[MyListCardManager] Card not found for poster download aid=%1").arg(aid));
        reply->deleteLater();
        return;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        LOG(QString("[MyListCardManager] Poster download error for aid=%1: %2")
            .arg(aid).arg(reply->errorString()));
        reply->deleteLater();
        return;
    }
    
    QByteArray imageData = reply->readAll();
    reply->deleteLater();
    
    if (imageData.isEmpty()) {
        LOG(QString("[MyListCardManager] Empty poster data for aid=%1").arg(aid));
        return;
    }
    
    // Load and set poster
    QPixmap poster;
    if (poster.loadFromData(imageData)) {
        card->setPoster(poster);
        
        // Remove from tracking
        QMutexLocker locker(&m_mutex);
        m_animeNeedingPoster.remove(aid);
        
        // Hide warning if metadata is also no longer needed
        bool stillNeedsData = m_animeNeedingMetadata.contains(aid);
        locker.unlock();
        
        if (!stillNeedsData) {
            card->setNeedsFetch(false);
        }
        
        // Store in database for future use
        QSqlDatabase db = QSqlDatabase::database();
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("UPDATE anime SET poster_image = ? WHERE aid = ?");
            q.addBindValue(imageData);
            q.addBindValue(aid);
            if (!q.exec()) {
                LOG(QString("[MyListCardManager] Failed to store poster for aid=%1: %2")
                    .arg(aid).arg(q.lastError().text()));
            }
        }
        
        emit cardUpdated(aid);
        LOG(QString("[MyListCardManager] Updated poster for aid=%1").arg(aid));
    } else {
        LOG(QString("[MyListCardManager] Failed to load poster image for aid=%1").arg(aid));
    }
}

void MyListCardManager::processBatchedUpdates()
{
    QSet<int> toUpdate;
    
    {
        QMutexLocker locker(&m_mutex);
        toUpdate = m_pendingCardUpdates;
        m_pendingCardUpdates.clear();
    }
    
    if (toUpdate.isEmpty()) {
        return;
    }
    
    LOG(QString("[MyListCardManager] Processing %1 batched card updates").arg(toUpdate.size()));
    
    for (const int aid : std::as_const(toUpdate)) {
        updateCardFromDatabase(aid);
    }
}

// Helper function to parse tag lists into TagInfo objects
static QList<AnimeCard::TagInfo> parseTags(const QString& tagNames, const QString& tagIds, const QString& tagWeights)
{
    QList<AnimeCard::TagInfo> tags;
    
    if (tagNames.isEmpty() || tagIds.isEmpty() || tagWeights.isEmpty()) {
        return tags;
    }
    
    QStringList names = tagNames.split(',');
    QStringList ids = tagIds.split(',');
    QStringList weights = tagWeights.split(',');
    
    // All three lists should have the same size
    int count = qMin(qMin(names.size(), ids.size()), weights.size());
    
    for (int i = 0; i < count; ++i) {
        AnimeCard::TagInfo tag;
        tag.name = names[i].trimmed();
        tag.id = ids[i].trimmed().toInt();
        tag.weight = weights[i].trimmed().toInt();
        tags.append(tag);
    }
    
    // Sort by weight (highest first)
    std::sort(tags.begin(), tags.end());
    
    return tags;
}

QString MyListCardManager::determineAnimeName(const QString& nameRomaji, const QString& nameEnglish, const QString& animeTitle, int aid)
{
    return AnimeUtils::determineAnimeName(nameRomaji, nameEnglish, animeTitle, aid);
}

QList<AnimeCard::TagInfo> MyListCardManager::getTagsOrCategoryFallback(const QString& tagNames, const QString& tagIds, const QString& tagWeights, const QString& category)
{
    QList<AnimeCard::TagInfo> tags = parseTags(tagNames, tagIds, tagWeights);
    
    if (!tags.isEmpty()) {
        return tags;
    }
    
    // Fallback to category if no tag data
    if (category.isEmpty()) {
        return tags;  // Empty list
    }
    
    QList<AnimeCard::TagInfo> categoryTags;
    QStringList categoryNames = category.split(',');
    int weight = 1000;  // Arbitrary high weight for category fallback
    
    for (const QString& catName : std::as_const(categoryNames)) {
        AnimeCard::TagInfo tag;
        tag.name = catName.trimmed();
        tag.id = 0;
        tag.weight = weight--;
        categoryTags.append(tag);
    }
    
    return categoryTags;
}

void MyListCardManager::updateCardAiredDates(AnimeCard* card, const QString& startDate, const QString& endDate)
{
    if (!startDate.isEmpty()) {
        aired airedDates(startDate, endDate);
        card->setAired(airedDates);
    } else {
        card->setAiredText("Unknown");
        
        // Track that this anime needs metadata if we have access to aid
        int aid = card->getAnimeId();
        if (aid > 0) {
            QMutexLocker locker(&m_mutex);
            m_animeNeedingMetadata.insert(aid);
        }
    }
}

AnimeCard* MyListCardManager::createCard(int aid)
{
    // Check if card already exists - prevent duplicates
    if (hasCard(aid)) {
        LOG(QString("[MyListCardManager] Card already exists for aid=%1, skipping duplicate creation").arg(aid));
        return m_cards[aid];
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return nullptr;
    }
    
    // Check if we have cached anime data (from bulk preload)
    bool hasAnimeData = false;
    QString animeName;
    QString typeName;
    QString startDate;
    QString endDate;
    QString picname;
    QByteArray posterData;
    QString category;
    QString rating;
    QString tagNameList;
    QString tagIdList;
    QString tagWeightList;
    bool isHidden = false;
    bool is18Restricted = false;
    int eptotal = 0;
    
    if (m_animeDataCache.contains(aid)) {
        // Use cached anime data (bulk preload scenario)
        const AnimeData& data = m_animeDataCache[aid];
        hasAnimeData = true;
        typeName = data.typeName;
        startDate = data.startDate;
        endDate = data.endDate;
        picname = data.picname;
        posterData = data.posterData;
        category = data.category;
        rating = data.rating;
        tagNameList = data.tagNameList;
        tagIdList = data.tagIdList;
        tagWeightList = data.tagWeightList;
        isHidden = data.isHidden;
        is18Restricted = data.is18Restricted;
        eptotal = data.eptotal;
        
        animeName = determineAnimeName(data.nameRomaji, data.nameEnglish, data.animeTitle, aid);
    } else {
        // No cached data, query anime table individually
        QSqlQuery q(db);
        q.prepare("SELECT a.nameromaji, a.nameenglish, a.eptotal, "
                  "at.title as anime_title, "
                  "a.eps, a.typename, a.startdate, a.enddate, a.picname, a.poster_image, a.category, "
                  "a.rating, a.tag_name_list, a.tag_id_list, a.tag_weight_list, a.hidden, a.is_18_restricted "
                  "FROM anime a "
                  "LEFT JOIN anime_titles at ON a.aid = at.aid AND at.type = 1 "
                  "WHERE a.aid = ?");
        q.addBindValue(aid);
        
        hasAnimeData = q.exec() && q.next();
        
        if (hasAnimeData) {
            // Have anime table data
            QString animeNameRomaji = q.value(0).toString();
            QString animeNameEnglish = q.value(1).toString();
            eptotal = q.value(2).toInt();
            QString animeTitle = q.value(3).toString();
            typeName = q.value(5).toString();
            startDate = q.value(6).toString();
            endDate = q.value(7).toString();
            picname = q.value(8).toString();
            posterData = q.value(9).toByteArray();
            category = q.value(10).toString();
            rating = q.value(11).toString();
            tagNameList = q.value(12).toString();
            tagIdList = q.value(13).toString();
            tagWeightList = q.value(14).toString();
            isHidden = q.value(15).toInt() == 1;
            is18Restricted = q.value(16).toInt() == 1;
            
            animeName = determineAnimeName(animeNameRomaji, animeNameEnglish, animeTitle, aid);
        }
    }
    
    if (!hasAnimeData) {
        // No anime table data, try to get basic info from cache or anime_titles
        if (m_animeTitlesCache.contains(aid)) {
            // Use preloaded cache (bulk loading scenario)
            animeName = m_animeTitlesCache[aid];
        } else {
            // Fallback to individual query (single card creation scenario)
            QSqlQuery titleQuery(db);
            titleQuery.prepare("SELECT title FROM anime_titles WHERE aid = ? AND type = 1 AND language = 'x-jat' LIMIT 1");
            titleQuery.addBindValue(aid);
            
            if (titleQuery.exec() && titleQuery.next()) {
                animeName = titleQuery.value(0).toString();
            } else {
                // Fallback to just showing the AID
                animeName = QString("Anime %1").arg(aid);
            }
        }
        
        LOG(QString("[MyListCardManager] Creating basic card for aid=%1 (no anime table data)").arg(aid));
    }
    
    // Create card
    AnimeCard *card = new AnimeCard(nullptr);
    card->setAnimeId(aid);
    card->setAnimeTitle(animeName);
    card->setHidden(isHidden);
    card->setIs18Restricted(is18Restricted);
    
    // Set type
    if (!typeName.isEmpty()) {
        card->setAnimeType(typeName);
    } else {
        card->setAnimeType("Unknown");
        m_animeNeedingMetadata.insert(aid);
    }
    
    // Set aired dates using helper
    updateCardAiredDates(card, startDate, endDate);
    
    // Set tags using helper (handles category fallback)
    QList<AnimeCard::TagInfo> tags = getTagsOrCategoryFallback(tagNameList, tagIdList, tagWeightList, category);
    if (!tags.isEmpty()) {
        card->setTags(tags);
    }
    
    // Set rating
    if (!rating.isEmpty()) {
        card->setRating(rating);
    }
    
    // Load poster asynchronously
    if (!posterData.isEmpty()) {
        // Defer poster loading to avoid blocking
        QMetaObject::invokeMethod(this, [this, card, posterData]() {
            QPixmap poster;
            if (poster.loadFromData(posterData)) {
                card->setPoster(poster);
            }
        }, Qt::QueuedConnection);
    } else if (!picname.isEmpty()) {
        m_animePicnames[aid] = picname;
        m_animeNeedingPoster.insert(aid);
        // Disabled auto-download - user can request via context menu
        // downloadPoster(aid, picname);
    } else {
        m_animeNeedingPoster.insert(aid);
        m_animeNeedingMetadata.insert(aid);
    }
    
    // Load episodes (skip for anime not in mylist - they have no episodes anyway)
    // If we have stats in cache, it means the anime is in mylist, so load episodes
    bool inStatsCache = m_statsCache.contains(aid);
    QString yesOrNo = inStatsCache ? "YES" : "NO";
    LOG(QString("[MyListCardManager] aid=%1: inStatsCache=%2, calling loadEpisodesForCard=%3")
        .arg(QString::number(aid), yesOrNo, yesOrNo));
    
    if (inStatsCache) {
        loadEpisodesForCard(card, aid);
    }
    
    // Calculate and set statistics (use cache if available)
    AnimeStats stats;
    if (m_statsCache.contains(aid)) {
        // Use cached statistics (bulk preload scenario)
        stats = m_statsCache[aid];
    } else {
        // Calculate statistics individually (single card creation scenario)
        stats = calculateStatistics(aid);
    }
    
    int totalNormalEpisodes = eptotal;
    if (totalNormalEpisodes <= 0) {
        totalNormalEpisodes = stats.normalEpisodes;
    }
    card->setStatistics(stats.normalEpisodes, totalNormalEpisodes, 
                       stats.normalViewed, stats.otherEpisodes, stats.otherViewed);
    
    // Add to cache first (before layout to avoid triggering layout updates prematurely)
    QMutexLocker locker(&m_mutex);
    m_cards[aid] = card;
    
    // Add to layout (deferred if in bulk loading mode)
    if (m_layout) {
        m_layout->addWidget(card);
    }
    locker.unlock();
    
    // Connect fetch data request signal from card
    connect(card, &AnimeCard::fetchDataRequested, this, &MyListCardManager::onFetchDataRequested);
    
    // Connect hide card request signal
    connect(card, &AnimeCard::hideCardRequested, this, &MyListCardManager::onHideCardRequested);
    
    // Connect mark episode watched signal
    connect(card, &AnimeCard::markEpisodeWatchedRequested, this, &MyListCardManager::onMarkEpisodeWatchedRequested);
    
    // Connect mark file watched signal
    connect(card, &AnimeCard::markFileWatchedRequested, this, &MyListCardManager::onMarkFileWatchedRequested);
    
    // Show warning indicator if metadata or poster is missing (instead of auto-fetching)
    if (m_animeNeedingMetadata.contains(aid) || m_animeNeedingPoster.contains(aid)) {
        card->setNeedsFetch(true);
    }
    
    emit cardCreated(aid, card);
    
    return card;
}

void MyListCardManager::updateCardFromDatabase(int aid)
{
    QMutexLocker locker(&m_mutex);
    AnimeCard *card = m_cards.value(aid, nullptr);
    locker.unlock();
    
    if (!card) {
        LOG(QString("[MyListCardManager] Card not found for update aid=%1").arg(aid));
        return;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    // Query updated anime data
    QSqlQuery q(db);
    q.prepare("SELECT a.nameromaji, a.nameenglish, "
              "at.title as anime_title, "
              "a.eps, a.typename, a.startdate, a.enddate, a.picname, a.poster_image, a.category, "
              "a.rating, a.tag_name_list, a.tag_id_list, a.tag_weight_list "
              "FROM anime a "
              "LEFT JOIN anime_titles at ON a.aid = at.aid AND at.type = 1 "
              "WHERE a.aid = ?");
    q.addBindValue(aid);
    
    if (!q.exec() || !q.next()) {
        LOG(QString("[MyListCardManager] Failed to query anime for update aid=%1: %2")
            .arg(aid).arg(q.lastError().text()));
        return;
    }
    
    QString animeName = q.value(0).toString();
    QString animeNameEnglish = q.value(1).toString();
    QString animeTitle = q.value(2).toString();
    int eps = q.value(3).toInt();
    QString typeName = q.value(4).toString();
    QString startDate = q.value(5).toString();
    QString endDate = q.value(6).toString();
    QString picname = q.value(7).toString();
    QByteArray posterData = q.value(8).toByteArray();
    QString category = q.value(9).toString();
    QString rating = q.value(10).toString();
    QString tagNameList = q.value(11).toString();
    QString tagIdList = q.value(12).toString();
    QString tagWeightList = q.value(13).toString();
    
    // Update anime name using helper
    animeName = determineAnimeName(animeName, animeNameEnglish, animeTitle, aid);
    if (!animeName.isEmpty()) {
        card->setAnimeTitle(animeName);
    }
    
    // Update type
    if (!typeName.isEmpty()) {
        card->setAnimeType(typeName);
    }
    
    // Update aired dates using helper
    if (!startDate.isEmpty()) {
        aired airedDates(startDate, endDate);
        card->setAired(airedDates);
    }
    
    // Update tags using helper (handles category fallback)
    QList<AnimeCard::TagInfo> tags = getTagsOrCategoryFallback(tagNameList, tagIdList, tagWeightList, category);
    if (!tags.isEmpty()) {
        card->setTags(tags);
    }
    
    // Update rating
    if (!rating.isEmpty()) {
        card->setRating(rating);
    }
    
    // Update poster if available
    if (!posterData.isEmpty()) {
        QPixmap poster;
        if (poster.loadFromData(posterData)) {
            card->setPoster(poster);
        }
    } else if (!picname.isEmpty() && !m_animePicnames.contains(aid)) {
        m_animePicnames[aid] = picname;
        downloadPoster(aid, picname);
    }
    
    // Reload episodes
    card->clearEpisodes();
    loadEpisodesForCard(card, aid);
    
    // Recalculate and update statistics
    AnimeStats stats = calculateStatistics(aid);
    int totalNormalEpisodes = (eps > 0) ? eps : stats.normalEpisodes;
    card->setStatistics(stats.normalEpisodes, totalNormalEpisodes,
                       stats.normalViewed, stats.otherEpisodes, stats.otherViewed);
    
    emit cardUpdated(aid);
    emit cardNeedsSorting(aid);
}

void MyListCardManager::loadEpisodesForCard(AnimeCard *card, int aid)
{
    // Group files by episode
    QMap<int, AnimeCard::EpisodeInfo> episodeMap;
    QMap<int, int> episodeFileCount;
    
    // Check if we have cached episode data
    if (m_episodesCache.contains(aid)) {
        // Use cached data (bulk loading scenario)
        const QList<EpisodeCacheEntry>& cachedEpisodes = m_episodesCache[aid];
        
        for (const EpisodeCacheEntry& entry : cachedEpisodes) {
            int lid = entry.lid;
            int eid = entry.eid;
            int fid = entry.fid;
            int state = entry.state;
            int viewed = entry.viewed;
            QString storage = entry.storage;
            QString episodeName = entry.episodeName;
            QString epno = entry.epno;
            QString filename = entry.filename;
            qint64 lastPlayed = entry.lastPlayed;
            QString localFilePath = entry.localFilePath;
            QString resolution = entry.resolution;
            QString quality = entry.quality;
            QString groupName = entry.groupName;
            int localWatched = entry.localWatched;
            
            // Get or create episode entry
            if (!episodeMap.contains(eid)) {
                AnimeCard::EpisodeInfo episodeInfo;
                episodeInfo.eid = eid;
                
                if (!epno.isEmpty()) {
                    episodeInfo.episodeNumber = ::epno(epno);
                }
                
                episodeInfo.episodeTitle = episodeName.isEmpty() ? "Episode" : episodeName;
                
                if (episodeName.isEmpty()) {
                    m_episodesNeedingData.insert(eid);
                }
                
                episodeMap[eid] = episodeInfo;
                episodeFileCount[eid] = 0;
            }
            
            // Create file info
            AnimeCard::FileInfo fileInfo;
            fileInfo.lid = lid;
            fileInfo.fid = fid;
            fileInfo.fileName = filename.isEmpty() ? QString("FID:%1").arg(fid) : filename;
            
            // State string
            switch(state) {
                case 0: fileInfo.state = "Unknown"; break;
                case 1: fileInfo.state = "HDD"; break;
                case 2: fileInfo.state = "CD/DVD"; break;
                case 3: fileInfo.state = "Deleted"; break;
                default: fileInfo.state = QString::number(state); break;
            }
            
            fileInfo.viewed = (viewed != 0);
            fileInfo.localWatched = (localWatched != 0);
            fileInfo.storage = !localFilePath.isEmpty() ? localFilePath : storage;
            fileInfo.localFilePath = localFilePath;  // Store local file path for existence check
            fileInfo.lastPlayed = lastPlayed;
            fileInfo.resolution = resolution;
            fileInfo.quality = quality;
            fileInfo.groupName = groupName;
            
            // Assign version number
            episodeFileCount[eid]++;
            fileInfo.version = episodeFileCount[eid];
            
            // Add file to episode
            episodeMap[eid].files.append(fileInfo);
        }
    } else {
        // No cached data, fall back to individual query (single card creation scenario)
        QSqlDatabase db = QSqlDatabase::database();
        if (!db.isOpen()) {
            return;
        }
        
        // Query episodes for this anime
        QSqlQuery episodeQuery(db);
        episodeQuery.prepare("SELECT m.lid, m.eid, m.fid, m.state, m.viewed, m.storage, "
                            "e.name as episode_name, e.epno, "
                            "f.filename, m.last_played, "
                            "lf.path as local_file_path, "
                            "f.resolution, f.quality, "
                            "g.name as group_name, "
                            "m.local_watched "
                            "FROM mylist m "
                            "LEFT JOIN episode e ON m.eid = e.eid "
                            "LEFT JOIN file f ON m.fid = f.fid "
                            "LEFT JOIN local_files lf ON m.local_file = lf.id "
                            "LEFT JOIN `group` g ON m.gid = g.gid "
                            "WHERE m.aid = ? "
                            "ORDER BY e.epno, m.lid");
        episodeQuery.addBindValue(aid);
        
        if (episodeQuery.exec()) {
            while (episodeQuery.next()) {
                int lid = episodeQuery.value(0).toInt();
                int eid = episodeQuery.value(1).toInt();
                int fid = episodeQuery.value(2).toInt();
                int state = episodeQuery.value(3).toInt();
                int viewed = episodeQuery.value(4).toInt();
                QString storage = episodeQuery.value(5).toString();
                QString episodeName = episodeQuery.value(6).toString();
                QString epno = episodeQuery.value(7).toString();
                QString filename = episodeQuery.value(8).toString();
                qint64 lastPlayed = episodeQuery.value(9).toLongLong();
                QString localFilePath = episodeQuery.value(10).toString();
                QString resolution = episodeQuery.value(11).toString();
                QString quality = episodeQuery.value(12).toString();
                QString groupName = episodeQuery.value(13).toString();
                int localWatched = episodeQuery.value(14).toInt();
                
                // Get or create episode entry
                if (!episodeMap.contains(eid)) {
                    AnimeCard::EpisodeInfo episodeInfo;
                    episodeInfo.eid = eid;
                    
                    if (!epno.isEmpty()) {
                        episodeInfo.episodeNumber = ::epno(epno);
                    }
                    
                    episodeInfo.episodeTitle = episodeName.isEmpty() ? "Episode" : episodeName;
                    
                    if (episodeName.isEmpty()) {
                        m_episodesNeedingData.insert(eid);
                    }
                    
                    episodeMap[eid] = episodeInfo;
                    episodeFileCount[eid] = 0;
                }
                
                // Create file info
                AnimeCard::FileInfo fileInfo;
                fileInfo.lid = lid;
                fileInfo.fid = fid;
                fileInfo.fileName = filename.isEmpty() ? QString("FID:%1").arg(fid) : filename;
                
                // State string
                switch(state) {
                    case 0: fileInfo.state = "Unknown"; break;
                    case 1: fileInfo.state = "HDD"; break;
                    case 2: fileInfo.state = "CD/DVD"; break;
                    case 3: fileInfo.state = "Deleted"; break;
                    default: fileInfo.state = QString::number(state); break;
                }
                
                fileInfo.viewed = (viewed != 0);
                fileInfo.localWatched = (localWatched != 0);
                fileInfo.storage = !localFilePath.isEmpty() ? localFilePath : storage;
                fileInfo.localFilePath = localFilePath;  // Store local file path for existence check
                fileInfo.lastPlayed = lastPlayed;
                fileInfo.resolution = resolution;
                fileInfo.quality = quality;
                fileInfo.groupName = groupName;
                
                // Assign version number
                episodeFileCount[eid]++;
                fileInfo.version = episodeFileCount[eid];
                
                // Add file to episode
                episodeMap[eid].files.append(fileInfo);
            }
        }
    }
    
    // Add all episodes to card in sorted order
    QList<AnimeCard::EpisodeInfo> episodeList = episodeMap.values();
    std::sort(episodeList.begin(), episodeList.end(), 
              [](const AnimeCard::EpisodeInfo& a, const AnimeCard::EpisodeInfo& b) {
        if (a.episodeNumber.isValid() && b.episodeNumber.isValid()) {
            return a.episodeNumber < b.episodeNumber;
        }
        if (!a.episodeNumber.isValid()) {
            return false;
        }
        if (!b.episodeNumber.isValid()) {
            return true;
        }
        return false;
    });
    
    for (const AnimeCard::EpisodeInfo& episodeInfo : std::as_const(episodeList)) {
        card->addEpisode(episodeInfo);
    }
}

void MyListCardManager::requestAnimeMetadata(int aid)
{
    if (adbapi) {
        LOG(QString("[MyListCardManager] Requesting metadata for anime %1").arg(aid));
        adbapi->Anime(aid);
    }
}

void MyListCardManager::downloadPoster(int aid, const QString &picname)
{
    if (picname.isEmpty()) {
        return;
    }
    
    // AniDB CDN URL for anime posters
    QString url = QString("http://img7.anidb.net/pics/anime/%1").arg(picname);
    
    LOG(QString("[MyListCardManager] Downloading poster for anime %1 from %2").arg(aid).arg(url));
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Usagi/1");
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    QMutexLocker locker(&m_mutex);
    m_posterDownloadRequests[reply] = aid;
}

MyListCardManager::AnimeStats MyListCardManager::calculateStatistics(int aid)
{
    AnimeStats stats = {0, 0, 0, 0, 0};
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return stats;
    }
    
    // Query to calculate statistics
    QSqlQuery q(db);
    q.prepare("SELECT e.epno, m.viewed "
              "FROM mylist m "
              "LEFT JOIN episode e ON m.eid = e.eid "
              "WHERE m.aid = ? "
              "GROUP BY m.eid");
    q.addBindValue(aid);
    
    QSet<int> normalEpisodes;
    QSet<int> otherEpisodes;
    QSet<int> viewedNormalEpisodes;
    QSet<int> viewedOtherEpisodes;
    
    if (q.exec()) {
        while (q.next()) {
            QString epnoStr = q.value(0).toString();
            int viewed = q.value(1).toInt();
            int eid = q.value(0).toInt(); // Using first column as eid placeholder
            
            if (!epnoStr.isEmpty()) {
                ::epno episodeNumber(epnoStr);
                int epType = episodeNumber.type();
                
                if (epType == 1) {
                    normalEpisodes.insert(eid);
                    if (viewed) {
                        viewedNormalEpisodes.insert(eid);
                    }
                } else if (epType > 1) {
                    otherEpisodes.insert(eid);
                    if (viewed) {
                        viewedOtherEpisodes.insert(eid);
                    }
                }
            } else {
                normalEpisodes.insert(eid);
                if (viewed) {
                    viewedNormalEpisodes.insert(eid);
                }
            }
        }
    }
    
    stats.normalEpisodes = normalEpisodes.size();
    stats.otherEpisodes = otherEpisodes.size();
    stats.normalViewed = viewedNormalEpisodes.size();
    stats.otherViewed = viewedOtherEpisodes.size();
    
    return stats;
}

void MyListCardManager::preloadAnimeDataCache(const QList<int>& aids)
{
    if (aids.isEmpty()) {
        return;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    // Build IN clause for bulk query
    QStringList aidStrings;
    for (int aid : aids) {
        aidStrings.append(QString::number(aid));
    }
    QString aidsList = aidStrings.join(",");
    
    // Bulk load anime table data
    QString query = QString("SELECT a.aid, a.nameromaji, a.nameenglish, a.eptotal, "
                           "at.title as anime_title, "
                           "a.typename, a.startdate, a.enddate, a.picname, a.poster_image, a.category, "
                           "a.rating, a.tag_name_list, a.tag_id_list, a.tag_weight_list, a.hidden, a.is_18_restricted "
                           "FROM anime a "
                           "LEFT JOIN anime_titles at ON a.aid = at.aid AND at.type = 1 "
                           "WHERE a.aid IN (%1)").arg(aidsList);
    
    QSqlQuery q(db);
    if (q.exec(query)) {
        while (q.next()) {
            int aid = q.value(0).toInt();
            AnimeData data;
            data.nameRomaji = q.value(1).toString();
            data.nameEnglish = q.value(2).toString();
            data.eptotal = q.value(3).toInt();
            data.animeTitle = q.value(4).toString();
            data.typeName = q.value(5).toString();
            data.startDate = q.value(6).toString();
            data.endDate = q.value(7).toString();
            data.picname = q.value(8).toString();
            data.posterData = q.value(9).toByteArray();
            data.category = q.value(10).toString();
            data.rating = q.value(11).toString();
            data.tagNameList = q.value(12).toString();
            data.tagIdList = q.value(13).toString();
            data.tagWeightList = q.value(14).toString();
            data.isHidden = q.value(15).toInt() == 1;
            data.is18Restricted = q.value(16).toInt() == 1;
            
            m_animeDataCache[aid] = data;
        }
    }
    
    // Bulk load statistics for all anime
    QString statsQuery = QString("SELECT m.aid, e.epno, m.viewed "
                                 "FROM mylist m "
                                 "LEFT JOIN episode e ON m.eid = e.eid "
                                 "WHERE m.aid IN (%1) "
                                 "ORDER BY m.aid").arg(aidsList);
    
    QSqlQuery statsQ(db);
    if (statsQ.exec(statsQuery)) {
        QMap<int, QSet<int>> normalEpisodesMap;
        QMap<int, QSet<int>> otherEpisodesMap;
        QMap<int, QSet<int>> viewedNormalEpisodesMap;
        QMap<int, QSet<int>> viewedOtherEpisodesMap;
        
        while (statsQ.next()) {
            int aid = statsQ.value(0).toInt();
            QString epnoStr = statsQ.value(1).toString();
            int viewed = statsQ.value(2).toInt();
            
            if (!epnoStr.isEmpty()) {
                ::epno episodeNumber(epnoStr);
                int epType = episodeNumber.type();
                int eid = statsQ.value(0).toInt(); // Using first column as placeholder
                
                if (epType == 1) {
                    normalEpisodesMap[aid].insert(eid);
                    if (viewed) {
                        viewedNormalEpisodesMap[aid].insert(eid);
                    }
                } else if (epType > 1) {
                    otherEpisodesMap[aid].insert(eid);
                    if (viewed) {
                        viewedOtherEpisodesMap[aid].insert(eid);
                    }
                }
            }
        }
        
        // Build stats cache - only for anime that actually have mylist entries
        // Collect all AIDs that appeared in the stats query
        QSet<int> aidsWithStats;
        for (auto it = normalEpisodesMap.constBegin(); it != normalEpisodesMap.constEnd(); ++it) {
            aidsWithStats.insert(it.key());
        }
        for (auto it = otherEpisodesMap.constBegin(); it != otherEpisodesMap.constEnd(); ++it) {
            aidsWithStats.insert(it.key());
        }
        
        for (int aid : aidsWithStats) {
            AnimeStats stats;
            stats.normalEpisodes = normalEpisodesMap[aid].size();
            stats.normalViewed = viewedNormalEpisodesMap[aid].size();
            stats.otherEpisodes = otherEpisodesMap[aid].size();
            stats.otherViewed = viewedOtherEpisodesMap[aid].size();
            stats.totalNormalEpisodes = 0; // Will be set from anime table or calculated
            
            m_statsCache[aid] = stats;
        }
    }
    
    LOG(QString("[MyListCardManager] Preloaded %1 anime data and %2 stats into cache")
        .arg(m_animeDataCache.size()).arg(m_statsCache.size()));
    
    // Debug: Log which AIDs are in stats cache
    if (!m_statsCache.isEmpty()) {
        QList<int> statsCacheAids = m_statsCache.keys();
        QStringList aidList;
        int count = 0;
        for (int aid : statsCacheAids) {
            if (count < 10) {  // Log first 10 for debugging
                aidList.append(QString::number(aid));
                count++;
            }
        }
        LOG(QString("[MyListCardManager] Stats cache contains anime: %1%2")
            .arg(aidList.join(", "),
                 statsCacheAids.size() > 10 ? QString(" and %1 more...").arg(statsCacheAids.size() - 10) : QString()));
    }
}

void MyListCardManager::preloadEpisodesCache(const QList<int>& aids)
{
    if (aids.isEmpty()) {
        return;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    // Build IN clause for bulk query
    QStringList aidStrings;
    for (int aid : aids) {
        aidStrings.append(QString::number(aid));
    }
    QString aidsList = aidStrings.join(",");
    
    // Clear existing cache
    m_episodesCache.clear();
    
    // Bulk load all episode data in a single query
    QString query = QString("SELECT m.aid, m.lid, m.eid, m.fid, m.state, m.viewed, m.storage, "
                           "e.name as episode_name, e.epno, "
                           "f.filename, m.last_played, "
                           "lf.path as local_file_path, "
                           "f.resolution, f.quality, "
                           "g.name as group_name, "
                           "m.local_watched "
                           "FROM mylist m "
                           "LEFT JOIN episode e ON m.eid = e.eid "
                           "LEFT JOIN file f ON m.fid = f.fid "
                           "LEFT JOIN local_files lf ON m.local_file = lf.id "
                           "LEFT JOIN `group` g ON m.gid = g.gid "
                           "WHERE m.aid IN (%1) "
                           "ORDER BY m.aid, e.epno, m.lid").arg(aidsList);
    
    QSqlQuery q(db);
    if (q.exec(query)) {
        while (q.next()) {
            int aid = q.value(0).toInt();
            
            EpisodeCacheEntry entry;
            entry.lid = q.value(1).toInt();
            entry.eid = q.value(2).toInt();
            entry.fid = q.value(3).toInt();
            entry.state = q.value(4).toInt();
            entry.viewed = q.value(5).toInt();
            entry.storage = q.value(6).toString();
            entry.episodeName = q.value(7).toString();
            entry.epno = q.value(8).toString();
            entry.filename = q.value(9).toString();
            entry.lastPlayed = q.value(10).toLongLong();
            entry.localFilePath = q.value(11).toString();
            entry.resolution = q.value(12).toString();
            entry.quality = q.value(13).toString();
            entry.groupName = q.value(14).toString();
            entry.localWatched = q.value(15).toInt();
            
            m_episodesCache[aid].append(entry);
        }
    }
    
    qint64 elapsed = timer.elapsed();
    LOG(QString("[MyListCardManager] Preloaded episode data for %1 anime (%2 total episodes) in %3 ms")
        .arg(m_episodesCache.size())
        .arg(std::accumulate(m_episodesCache.begin(), m_episodesCache.end(), 0,
            [](int sum, const QList<EpisodeCacheEntry>& episodes) { return sum + episodes.size(); }))
        .arg(elapsed));
}

void MyListCardManager::onHideCardRequested(int aid)
{
    LOG(QString("[MyListCardManager] Hide card requested for anime %1").arg(aid));
    
    QMutexLocker locker(&m_mutex);
    AnimeCard *card = m_cards.value(aid, nullptr);
    
    if (!card) {
        LOG(QString("[MyListCardManager] Card not found for hide request aid=%1").arg(aid));
        return;
    }
    
    // Toggle hidden state
    bool isHidden = card->isHidden();
    card->setHidden(!isHidden);
    
    // Update database to persist hidden state
    locker.unlock();
    
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        QSqlQuery q(db);
        
        // Update hidden state
        q.prepare("UPDATE anime SET hidden = ? WHERE aid = ?");
        q.addBindValue(!isHidden ? 1 : 0);
        q.addBindValue(aid);
        if (!q.exec()) {
            LOG(QString("[MyListCardManager] Failed to update hidden state for aid=%1: %2")
                .arg(aid).arg(q.lastError().text()));
        } else {
            LOG(QString("[MyListCardManager] Updated hidden state for aid=%1 to %2").arg(aid).arg(!isHidden));
            // Trigger card sorting/repositioning
            emit cardNeedsSorting(aid);
        }
    }
}

void MyListCardManager::onMarkEpisodeWatchedRequested(int eid)
{
    LOG(QString("[MyListCardManager] Mark episode watched requested for eid=%1").arg(eid));
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    // Get current timestamp for viewdate
    qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();
    
    // Update all files for this episode to viewed=1 and local_watched=1 with viewdate
    QSqlQuery q(db);
    q.prepare("UPDATE mylist SET viewed = 1, local_watched = 1, viewdate = ? WHERE eid = ?");
    q.addBindValue(currentTimestamp);
    q.addBindValue(eid);
    
    if (!q.exec()) {
        LOG(QString("[MyListCardManager] Failed to mark episode watched eid=%1: %2")
            .arg(eid).arg(q.lastError().text()));
        return;
    }
    
    int rowsAffected = q.numRowsAffected();
    LOG(QString("[MyListCardManager] Marked %1 file(s) as watched for episode eid=%2").arg(rowsAffected).arg(eid));
    
    // Get all files for this episode to update API
    q.prepare("SELECT m.lid, f.size, f.ed2k, m.aid FROM mylist m "
              "INNER JOIN file f ON m.fid = f.fid "
              "WHERE m.eid = ?");
    q.addBindValue(eid);
    
    if (!q.exec()) {
        LOG(QString("[MyListCardManager] Failed to query files for episode eid=%1: %2")
            .arg(eid).arg(q.lastError().text()));
        return;
    }
    
    int aid = 0;
    // Emit API update requests for each file
    while (q.next()) {
        int lid = q.value(0).toInt();
        int size = q.value(1).toInt();
        QString ed2k = q.value(2).toString();
        aid = q.value(3).toInt();
        
        // Emit signal for API update (will be handled by Window to call anidb->UpdateFile)
        emit fileNeedsApiUpdate(lid, size, ed2k, 1);
    }
    
    if (aid > 0) {
        LOG(QString("[MyListCardManager] Requesting play button update for aid=%1").arg(aid));
        // Emit signal to update play buttons in tree view
        emit playButtonsNeedUpdate(aid);
        
        // Update the card immediately to reflect the change
        updateCardFromDatabase(aid);
    }
}

void MyListCardManager::onMarkFileWatchedRequested(int lid)
{
    LOG(QString("[MyListCardManager] Mark file watched requested for lid=%1").arg(lid));
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    // Get current timestamp for viewdate
    qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();
    
    // Update this specific file to viewed=1 and local_watched=1 with viewdate
    QSqlQuery q(db);
    q.prepare("UPDATE mylist SET viewed = 1, local_watched = 1, viewdate = ? WHERE lid = ?");
    q.addBindValue(currentTimestamp);
    q.addBindValue(lid);
    
    if (!q.exec()) {
        LOG(QString("[MyListCardManager] Failed to mark file watched lid=%1: %2")
            .arg(lid).arg(q.lastError().text()));
        return;
    }
    
    LOG(QString("[MyListCardManager] Marked file lid=%1 as watched").arg(lid));
    
    // Get file info for API update
    q.prepare("SELECT m.aid, f.size, f.ed2k FROM mylist m "
              "INNER JOIN file f ON m.fid = f.fid "
              "WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (!q.exec() || !q.next()) {
        LOG(QString("[MyListCardManager] Failed to find file info for lid=%1").arg(lid));
        return;
    }
    
    int aid = q.value(0).toInt();
    int size = q.value(1).toInt();
    QString ed2k = q.value(2).toString();
    
    LOG(QString("[MyListCardManager] Marked file lid=%1 as watched, updating card for aid=%2").arg(lid).arg(aid));
    
    // Emit signal for API update (will be handled by Window to call anidb->UpdateFile)
    emit fileNeedsApiUpdate(lid, size, ed2k, 1);
    
    // Emit signal to update play buttons in tree view
    emit playButtonsNeedUpdate(aid);
    
    // Update the card immediately to reflect the change
    updateCardFromDatabase(aid);
}
