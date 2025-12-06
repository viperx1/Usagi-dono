#ifndef TAGINFO_H
#define TAGINFO_H

#include <QString>

/**
 * @class TagInfo
 * @brief Represents an anime tag with its name, ID, and weight
 * 
 * Tags are used to categorize anime with weighted importance.
 * Higher weight indicates more relevance to the anime.
 * 
 * SOLID Principles Applied:
 * - Single Responsibility: Only manages tag information
 * - Encapsulation: Private fields with controlled access
 * - Comparison: Proper operator for sorting by weight
 */
class TagInfo
{
public:
    /**
     * @brief Construct an empty TagInfo
     */
    TagInfo();
    
    /**
     * @brief Construct a TagInfo with all fields
     * @param name Tag name
     * @param id Tag ID
     * @param weight Tag weight (higher = more relevant)
     */
    TagInfo(const QString& name, int id, int weight);
    
    // Getters
    QString name() const { return m_name; }
    int id() const { return m_id; }
    int weight() const { return m_weight; }
    
    // Setters
    void setName(const QString& name) { m_name = name; }
    void setId(int id) { m_id = id; }
    void setWeight(int weight) { m_weight = weight; }
    
    /**
     * @brief Compare tags by weight for sorting (higher weight first)
     * @param other Other tag to compare with
     * @return true if this tag has higher weight
     */
    bool operator<(const TagInfo& other) const {
        return m_weight > other.m_weight;
    }
    
    /**
     * @brief Check if tag has valid data
     * @return true if name is not empty and id is valid
     */
    bool isValid() const;
    
    /**
     * @brief Reset to empty state
     */
    void reset();
    
private:
    QString m_name;    ///< Tag name
    int m_id;          ///< Tag ID
    int m_weight;      ///< Tag weight (relevance score)
};

#endif // TAGINFO_H
