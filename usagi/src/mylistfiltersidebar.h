#ifndef MYLISTFILTERSIDEBAR_H
#define MYLISTFILTERSIDEBAR_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>

/**
 * MyListFilterSidebar - Sidebar widget for filtering anime cards
 * 
 * Provides search and filter options for the mylist view:
 * - Search by anime title and alternative titles
 * - Filter by anime type (TV, Movie, OVA, etc.)
 * - Filter by completion status (completed, watching, etc.)
 * - Filter by viewed status
 * - Session management settings (ahead buffer, deletion threshold)
 * 
 * All filtering is done on data already loaded in memory (no database operations)
 */
class MyListFilterSidebar : public QWidget
{
    Q_OBJECT
    
public:
    explicit MyListFilterSidebar(QWidget *parent = nullptr);
    virtual ~MyListFilterSidebar();
    
    // Get current filter values
    QString getSearchText() const;
    QString getTypeFilter() const;
    QString getCompletionFilter() const;
    bool getShowOnlyUnwatched() const;
    int getSortIndex() const;
    bool getSortAscending() const;
    QString getAdultContentFilter() const;
    bool getInMyListOnly() const;
    
    // Session settings getters
    int getAheadBuffer() const;
    int getDeletionThresholdType() const;  // 0=FixedGB, 1=Percentage
    double getDeletionThresholdValue() const;
    bool isAutoMarkDeletionEnabled() const;
    
    // Session settings setters (for loading saved values)
    void setAheadBuffer(int episodes);
    void setDeletionThresholdType(int type);
    void setDeletionThresholdValue(double value);
    void setAutoMarkDeletionEnabled(bool enabled);
    
    // Reset all filters
    void resetFilters();
    
signals:
    // Emitted when any filter changes
    void filterChanged();
    
    // Emitted when sort options change
    void sortChanged();
    
    // Emitted when session settings change
    void sessionSettingsChanged();
    
private slots:
    void onSearchTextChanged();
    void onFilterChanged();
    void onSortChanged();
    void onSortOrderToggled();
    void onSessionSettingsChanged();
    
private:
    void setupUI();
    
    // UI elements - Filters
    QLineEdit *m_searchField;
    QComboBox *m_typeFilter;
    QComboBox *m_completionFilter;
    QCheckBox *m_showOnlyUnwatchedCheckbox;
    QCheckBox *m_inMyListCheckbox;
    QComboBox *m_sortComboBox;
    QPushButton *m_sortOrderButton;
    QComboBox *m_adultContentFilter;
    QPushButton *m_resetButton;
    
    // UI elements - Session settings
    QSpinBox *m_aheadBufferSpinBox;
    QComboBox *m_thresholdTypeComboBox;
    QDoubleSpinBox *m_thresholdValueSpinBox;
    QCheckBox *m_autoMarkDeletionCheckbox;
    
    bool m_sortAscending;
};

#endif // MYLISTFILTERSIDEBAR_H
