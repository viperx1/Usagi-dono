#ifndef ANIMEFILTER_H
#define ANIMEFILTER_H

#include <QString>
#include <QList>
#include "animemetadatacache.h"

// Forward declarations
class AnimeCard;
class CachedAnimeData;

/**
 * Unified data accessor for anime filtering
 * 
 * Abstracts the difference between getting data from AnimeCard vs cached data,
 * eliminating the repeated "if (card) ... else ..." pattern throughout filtering code.
 * 
 * This follows the Single Responsibility Principle - one class responsible for
 * data access during filtering.
 */
class AnimeDataAccessor
{
public:
    AnimeDataAccessor(int aid, AnimeCard* card, const CachedAnimeData& cachedData);
    
    // Anime identification
    int getAnimeId() const { return m_aid; }
    
    // Basic info
    QString getTitle() const;
    QString getType() const;
    bool is18Restricted() const;
    
    // Episode data
    int getNormalEpisodes() const;
    int getNormalViewed() const;
    int getOtherEpisodes() const;
    int getOtherViewed() const;
    int getTotalEpisodes() const;
    
    // Calculated properties
    bool hasData() const;
    
private:
    int m_aid;
    AnimeCard* m_card;
    const CachedAnimeData& m_cachedData;
};

/**
 * Abstract base class for anime filters
 * 
 * Follows the Interface Segregation Principle - clients depend only on the
 * filter interface they need, not on concrete implementations.
 * 
 * Each filter type implements this interface, making them:
 * - Testable in isolation
 * - Composable (can combine multiple filters)
 * - Maintainable (each filter in its own class)
 */
class AnimeFilter
{
public:
    virtual ~AnimeFilter() = default;
    
    /**
     * Check if anime matches this filter
     * @param accessor Unified access to anime data
     * @return true if anime passes this filter
     */
    virtual bool matches(const AnimeDataAccessor& accessor) const = 0;
    
    /**
     * Get human-readable description of this filter
     * Useful for debugging and logging
     */
    virtual QString description() const = 0;
};

/**
 * Filter by search text in anime title or alternative titles
 */
class SearchFilter : public AnimeFilter
{
public:
    SearchFilter(const QString& searchText, const AnimeMetadataCache* cache);
    
    bool matches(const AnimeDataAccessor& accessor) const override;
    QString description() const override;
    
private:
    QString m_searchText;
    const AnimeMetadataCache* m_cache;
};

/**
 * Filter by anime type (TV Series, Movie, OVA, etc.)
 */
class TypeFilter : public AnimeFilter
{
public:
    explicit TypeFilter(const QString& typeFilter);
    
    bool matches(const AnimeDataAccessor& accessor) const override;
    QString description() const override;
    
private:
    QString m_typeFilter;
};

/**
 * Filter by completion status (Completed, Watching, Not Started)
 */
class CompletionFilter : public AnimeFilter
{
public:
    explicit CompletionFilter(const QString& completionFilter);
    
    bool matches(const AnimeDataAccessor& accessor) const override;
    QString description() const override;
    
private:
    QString m_completionFilter;
    
    // Helper to determine completion status
    enum class CompletionStatus {
        NotStarted,
        Watching,
        Completed
    };
    
    CompletionStatus getCompletionStatus(int normalViewed, int normalEpisodes, int totalEpisodes) const;
};

/**
 * Filter to show only anime with unwatched episodes
 */
class UnwatchedFilter : public AnimeFilter
{
public:
    explicit UnwatchedFilter(bool enabled);
    
    bool matches(const AnimeDataAccessor& accessor) const override;
    QString description() const override;
    
private:
    bool m_enabled;
};

/**
 * Filter by adult content (18+)
 * Options: "hide", "showonly", "ignore"
 */
class AdultContentFilter : public AnimeFilter
{
public:
    explicit AdultContentFilter(const QString& filterMode);
    
    bool matches(const AnimeDataAccessor& accessor) const override;
    QString description() const override;
    
private:
    QString m_filterMode;
};

/**
 * Composite filter - combines multiple filters with AND logic
 * 
 * Follows the Composite Pattern - allows treating a group of filters
 * the same way as a single filter.
 * 
 * All filters must pass for the composite to pass.
 */
class CompositeFilter : public AnimeFilter
{
public:
    CompositeFilter() = default;
    ~CompositeFilter() override;
    
    // Add a filter to the composite (takes ownership)
    void addFilter(AnimeFilter* filter);
    
    // Remove all filters
    void clear();
    
    bool matches(const AnimeDataAccessor& accessor) const override;
    QString description() const override;
    
    // Get count of active filters
    int count() const { return m_filters.size(); }
    
private:
    QList<AnimeFilter*> m_filters;
};

#endif // ANIMEFILTER_H
