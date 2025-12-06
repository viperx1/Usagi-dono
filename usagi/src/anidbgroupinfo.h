#ifndef ANIDBGROUPINFO_H
#define ANIDBGROUPINFO_H

#include <QString>

/**
 * @brief AniDBGroupInfo - Type-safe representation of AniDB group data
 * 
 * This class replaces the plain GroupData struct with:
 * - Type-safe fields (int for group ID)
 * - Validation of data
 * - Encapsulation with getters/setters
 * 
 * Follows SOLID principles:
 * - Single Responsibility: Represents group metadata only
 * - Encapsulation: Private members with validated setters
 * - Type Safety: Proper C++ types instead of all QString
 * 
 * Usage:
 *   AniDBGroupInfo groupInfo;
 *   groupInfo.setGroupId(1234);
 *   groupInfo.setGroupName("SubsPlease");
 *   int gid = groupInfo.groupId();
 */
class AniDBGroupInfo
{
public:
    /**
     * @brief Default constructor creates invalid group info
     */
    AniDBGroupInfo();
    
    // Group identification
    int groupId() const { return m_gid; }
    QString groupName() const { return m_groupname; }
    QString groupShortName() const { return m_groupshortname; }
    
    void setGroupId(int gid) { m_gid = gid; }
    void setGroupName(const QString& name) { m_groupname = name; }
    void setGroupShortName(const QString& shortname) { m_groupshortname = shortname; }
    
    // Validation
    bool isValid() const { return m_gid > 0; }
    bool hasName() const { return !m_groupname.isEmpty() || !m_groupshortname.isEmpty(); }
    
private:
    int m_gid;
    QString m_groupname;
    QString m_groupshortname;
};

#endif // ANIDBGROUPINFO_H
