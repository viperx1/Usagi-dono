#include "animefilter.h"
#include "animecard.h"
#include "mylistcardmanager.h"
#include "cachedanimedata.h"

// ============================================================================
// AnimeDataAccessor Implementation
// ============================================================================

AnimeDataAccessor::AnimeDataAccessor(int aid, AnimeCard* card, 
                                     const CachedAnimeData& cachedData)
    : m_aid(aid)
    , m_card(card)
    , m_cachedData(cachedData)
{
}

QString AnimeDataAccessor::getTitle() const
{
    return m_card ? m_card->getAnimeTitle() : m_cachedData.animeName();
}

QString AnimeDataAccessor::getType() const
{
    return m_card ? m_card->getAnimeType() : m_cachedData.typeName();
}

bool AnimeDataAccessor::is18Restricted() const
{
    return m_card ? m_card->is18Restricted() : m_cachedData.is18Restricted();
}

int AnimeDataAccessor::getNormalEpisodes() const
{
    return m_card ? m_card->getNormalEpisodes() : m_cachedData.stats().normalEpisodes();
}

int AnimeDataAccessor::getNormalViewed() const
{
    return m_card ? m_card->getNormalViewed() : m_cachedData.stats().normalViewed();
}

int AnimeDataAccessor::getOtherEpisodes() const
{
    return m_card ? m_card->getOtherEpisodes() : m_cachedData.stats().otherEpisodes();
}

int AnimeDataAccessor::getOtherViewed() const
{
    return m_card ? m_card->getOtherViewed() : m_cachedData.stats().otherViewed();
}

int AnimeDataAccessor::getTotalEpisodes() const
{
    if (m_card) {
        return m_card->getTotalNormalEpisodes();
    } else {
        return m_cachedData.eptotal() > 0 ? m_cachedData.eptotal() : m_cachedData.stats().totalNormalEpisodes();
    }
}

bool AnimeDataAccessor::hasData() const
{
    return m_card != nullptr || m_cachedData.hasData();
}

// ============================================================================
// SearchFilter Implementation
// ============================================================================

SearchFilter::SearchFilter(const QString& searchText, const AnimeMetadataCache* cache)
    : m_searchText(searchText)
    , m_cache(cache)
{
}

bool SearchFilter::matches(const AnimeDataAccessor& accessor) const
{
    if (m_searchText.isEmpty()) {
        return true;
    }
    
    // Check main title
    if (accessor.getTitle().contains(m_searchText, Qt::CaseInsensitive)) {
        return true;
    }
    
    // Check alternative titles from cache
    if (m_cache) {
        return m_cache->matchesAnyTitle(accessor.getAnimeId(), m_searchText);
    }
    
    return false;
}

QString SearchFilter::description() const
{
    return m_searchText.isEmpty() 
        ? QString("No search filter") 
        : QString("Search: \"%1\"").arg(m_searchText);
}

// ============================================================================
// TypeFilter Implementation
// ============================================================================

TypeFilter::TypeFilter(const QString& typeFilter)
    : m_typeFilter(typeFilter)
{
}

bool TypeFilter::matches(const AnimeDataAccessor& accessor) const
{
    if (m_typeFilter.isEmpty()) {
        return true;
    }
    
    return accessor.getType() == m_typeFilter;
}

QString TypeFilter::description() const
{
    return m_typeFilter.isEmpty() 
        ? QString("All types") 
        : QString("Type: %1").arg(m_typeFilter);
}

// ============================================================================
// CompletionFilter Implementation
// ============================================================================

CompletionFilter::CompletionFilter(const QString& completionFilter)
    : m_completionFilter(completionFilter)
{
}

CompletionFilter::CompletionStatus CompletionFilter::getCompletionStatus(
    int normalViewed, int normalEpisodes, int totalEpisodes) const
{
    // Not started: no episodes viewed
    if (normalViewed == 0) {
        return CompletionStatus::NotStarted;
    }
    
    // Determine which total to use
    // If anime has a known total (from AniDB), use that
    // Otherwise, use the count of episodes in mylist
    int effectiveTotal = (totalEpisodes > 0) ? totalEpisodes : normalEpisodes;
    
    // Completed: all episodes viewed
    // Note: effectiveTotal > 0 check ensures we don't mark anime with no episodes as completed
    if (normalViewed >= effectiveTotal && effectiveTotal > 0) {
        return CompletionStatus::Completed;
    }
    
    // Watching: some but not all viewed
    return CompletionStatus::Watching;
}

bool CompletionFilter::matches(const AnimeDataAccessor& accessor) const
{
    if (m_completionFilter.isEmpty()) {
        return true;
    }
    
    int normalViewed = accessor.getNormalViewed();
    int normalEpisodes = accessor.getNormalEpisodes();
    int totalEpisodes = accessor.getTotalEpisodes();
    
    CompletionStatus status = getCompletionStatus(normalViewed, normalEpisodes, totalEpisodes);
    
    if (m_completionFilter == "completed") {
        return status == CompletionStatus::Completed;
    } else if (m_completionFilter == "watching") {
        return status == CompletionStatus::Watching;
    } else if (m_completionFilter == "notstarted") {
        return status == CompletionStatus::NotStarted;
    }
    
    return false;
}

QString CompletionFilter::description() const
{
    if (m_completionFilter.isEmpty()) {
        return QString("All completion statuses");
    } else if (m_completionFilter == "completed") {
        return QString("Completed");
    } else if (m_completionFilter == "watching") {
        return QString("Watching");
    } else if (m_completionFilter == "notstarted") {
        return QString("Not started");
    }
    return QString("Unknown completion filter");
}

// ============================================================================
// UnwatchedFilter Implementation
// ============================================================================

UnwatchedFilter::UnwatchedFilter(bool enabled)
    : m_enabled(enabled)
{
}

bool UnwatchedFilter::matches(const AnimeDataAccessor& accessor) const
{
    if (!m_enabled) {
        return true;
    }
    
    int normalEpisodes = accessor.getNormalEpisodes();
    int normalViewed = accessor.getNormalViewed();
    int otherEpisodes = accessor.getOtherEpisodes();
    int otherViewed = accessor.getOtherViewed();
    
    // Show only if there are unwatched episodes (normal or other)
    return (normalEpisodes > normalViewed) || (otherEpisodes > otherViewed);
}

QString UnwatchedFilter::description() const
{
    return m_enabled 
        ? QString("Show only with unwatched episodes") 
        : QString("Show all (watched and unwatched)");
}

// ============================================================================
// AdultContentFilter Implementation
// ============================================================================

AdultContentFilter::AdultContentFilter(const QString& filterMode)
    : m_filterMode(filterMode)
{
}

bool AdultContentFilter::matches(const AnimeDataAccessor& accessor) const
{
    bool is18 = accessor.is18Restricted();
    
    if (m_filterMode == "hide") {
        // Hide 18+ content
        return !is18;
    } else if (m_filterMode == "showonly") {
        // Show only 18+ content
        return is18;
    }
    
    // "ignore" or any other value means no filtering
    return true;
}

QString AdultContentFilter::description() const
{
    if (m_filterMode == "hide") {
        return QString("Hide 18+ content");
    } else if (m_filterMode == "showonly") {
        return QString("Show only 18+ content");
    }
    return QString("Ignore adult content filter");
}

// ============================================================================
// CompositeFilter Implementation
// ============================================================================

CompositeFilter::~CompositeFilter()
{
    clear();
}

void CompositeFilter::addFilter(AnimeFilter* filter)
{
    if (filter) {
        m_filters.append(filter);
    }
}

void CompositeFilter::clear()
{
    qDeleteAll(m_filters);
    m_filters.clear();
}

bool CompositeFilter::matches(const AnimeDataAccessor& accessor) const
{
    // All filters must pass (AND logic)
    for (const AnimeFilter* filter : m_filters) {
        if (filter && !filter->matches(accessor)) {
            return false;
        }
    }
    return true;
}

QString CompositeFilter::description() const
{
    if (m_filters.isEmpty()) {
        return QString("No filters active");
    }
    
    QStringList descriptions;
    for (const AnimeFilter* filter : m_filters) {
        if (filter) {
            descriptions.append(filter->description());
        }
    }
    
    return descriptions.join(" AND ");
}
