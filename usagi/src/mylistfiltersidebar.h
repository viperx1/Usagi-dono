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
 * 
 * Note: Session manager settings have been moved to the Settings tab.
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
    bool getShowMarkedForDeletion() const;
    int getSortIndex() const;
    bool getSortAscending() const;
    QString getAdultContentFilter() const;
    bool getInMyListOnly() const;
    bool getShowSeriesChain() const;
    
    // Set filter values (for loading from settings)
    void setSortIndex(int index);
    void setSortAscending(bool ascending);
    void setTypeFilter(const QString& typeData);
    void setCompletionFilter(const QString& completionData);
    void setShowOnlyUnwatched(bool checked);
    void setShowMarkedForDeletion(bool checked);
    void setInMyListOnly(bool checked);
    void setShowSeriesChain(bool checked);
    void setAdultContentFilter(const QString& filterData);
    
    // Reset all filters
    void resetFilters();
    
    // Set the toggle button for external use
    void setToggleButton(QPushButton *button);
    
signals:
    // Emitted when any filter changes
    void filterChanged();
    
    // Emitted when sort options change
    void sortChanged();
    
    // Emitted when collapse button is clicked
    void collapseRequested();
    
private slots:
    void onSearchTextChanged();
    void onFilterChanged();
    void onSortChanged();
    void onSortOrderToggled();
    void onSeriesChainToggled();
    
protected:
    void resizeEvent(QResizeEvent *event) override;
    
private:
    void setupUI();
    void updateSortControlsState();
    void setComboBoxByData(QComboBox* comboBox, const QString& data);
    
    // UI elements - Filters
    QLineEdit *m_searchField;
    QComboBox *m_typeFilter;
    QComboBox *m_completionFilter;
    QCheckBox *m_showOnlyUnwatchedCheckbox;
    QCheckBox *m_showMarkedForDeletionCheckbox;
    QCheckBox *m_inMyListCheckbox;
    QCheckBox *m_showSeriesChainCheckbox;
    QComboBox *m_sortComboBox;
    QPushButton *m_sortOrderButton;
    QComboBox *m_adultContentFilter;
    QPushButton *m_resetButton;
    QPushButton *m_collapseButton;
    
    bool m_sortAscending;
};

#endif // MYLISTFILTERSIDEBAR_H
