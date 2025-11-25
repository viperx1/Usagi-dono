#ifndef MYLISTFILTERSIDEBAR_H
#define MYLISTFILTERSIDEBAR_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QPushButton>

/**
 * MyListFilterSidebar - Sidebar widget for filtering anime cards
 * 
 * Provides search and filter options for the mylist view:
 * - Search by anime title and alternative titles
 * - Filter by anime type (TV, Movie, OVA, etc.)
 * - Filter by completion status (completed, watching, etc.)
 * - Filter by viewed status
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
    
    // Reset all filters
    void resetFilters();
    
signals:
    // Emitted when any filter changes
    void filterChanged();
    
    // Emitted when sort options change
    void sortChanged();
    
private slots:
    void onSearchTextChanged();
    void onFilterChanged();
    void onSortChanged();
    void onSortOrderToggled();
    
private:
    void setupUI();
    
    // UI elements
    QLineEdit *m_searchField;
    QComboBox *m_typeFilter;
    QComboBox *m_completionFilter;
    QCheckBox *m_showOnlyUnwatchedCheckbox;
    QComboBox *m_sortComboBox;
    QPushButton *m_sortOrderButton;
    QComboBox *m_adultContentFilter;
    QPushButton *m_resetButton;
    
    bool m_sortAscending;
};

#endif // MYLISTFILTERSIDEBAR_H
