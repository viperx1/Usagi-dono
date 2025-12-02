#include "mylistfiltersidebar.h"
#include <QLabel>
#include <QTimer>
#include <QResizeEvent>

MyListFilterSidebar::MyListFilterSidebar(QWidget *parent)
    : QWidget(parent)
    , m_sortAscending(false)  // Default to descending (newest first for aired date)
{
    setupUI();
}

MyListFilterSidebar::~MyListFilterSidebar()
{
}

void MyListFilterSidebar::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(10);
    
    // Title - centered
    QLabel *titleLabel = new QLabel("<b>Search & Filter</b>");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // Collapse button - will be positioned absolutely on top of the title area
    m_collapseButton = new QPushButton("◀", this);
    m_collapseButton->setMaximumWidth(30);
    m_collapseButton->setMaximumHeight(30);
    m_collapseButton->setToolTip("Hide filter sidebar");
    m_collapseButton->raise();  // Bring to front
    connect(m_collapseButton, &QPushButton::clicked, this, &MyListFilterSidebar::collapseRequested);
    
    // Search group
    QGroupBox *searchGroup = new QGroupBox("Search");
    QVBoxLayout *searchLayout = new QVBoxLayout(searchGroup);
    
    QLabel *searchLabel = new QLabel("Anime Title:");
    m_searchField = new QLineEdit();
    m_searchField->setPlaceholderText("Search by title or alternative title...");
    m_searchField->setClearButtonEnabled(true);
    
    // Use a timer to debounce search input (avoid filtering on every keystroke)
    QTimer *searchDebounceTimer = new QTimer(this);
    searchDebounceTimer->setSingleShot(true);
    searchDebounceTimer->setInterval(300); // 300ms delay
    
    connect(m_searchField, &QLineEdit::textChanged, this, [this, searchDebounceTimer]() {
        searchDebounceTimer->start();
    });
    connect(searchDebounceTimer, &QTimer::timeout, this, &MyListFilterSidebar::onSearchTextChanged);
    
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchField);
    mainLayout->addWidget(searchGroup);
    
    // Sorting group
    QGroupBox *sortGroup = new QGroupBox("Sort");
    QVBoxLayout *sortLayout = new QVBoxLayout(sortGroup);
    
    QLabel *sortLabel = new QLabel("Sort by:");
    m_sortComboBox = new QComboBox();
    m_sortComboBox->addItem("Anime Title");
    m_sortComboBox->addItem("Type");
    m_sortComboBox->addItem("Aired Date");
    m_sortComboBox->addItem("Episodes (Count)");
    m_sortComboBox->addItem("Completion %");
    m_sortComboBox->addItem("Last Played");
    m_sortComboBox->setCurrentIndex(2);  // Default to Aired Date
    connect(m_sortComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MyListFilterSidebar::onSortChanged);
    
    m_sortOrderButton = new QPushButton("↓ Desc");
    m_sortOrderButton->setMaximumWidth(80);
    m_sortOrderButton->setToolTip("Toggle sort order (ascending/descending)");
    connect(m_sortOrderButton, &QPushButton::clicked, this, &MyListFilterSidebar::onSortOrderToggled);
    
    QHBoxLayout *sortOrderLayout = new QHBoxLayout();
    sortOrderLayout->addWidget(m_sortComboBox, 1);
    sortOrderLayout->addWidget(m_sortOrderButton);
    
    sortLayout->addWidget(sortLabel);
    sortLayout->addLayout(sortOrderLayout);
    mainLayout->addWidget(sortGroup);
    
    // Filter by type group
    QGroupBox *typeGroup = new QGroupBox("Type");
    QVBoxLayout *typeLayout = new QVBoxLayout(typeGroup);
    
    QLabel *typeLabel = new QLabel("Anime Type:");
    m_typeFilter = new QComboBox();
    m_typeFilter->addItem("All Types", "");
    m_typeFilter->addItem("TV Series", "TV Series");
    m_typeFilter->addItem("Movie", "Movie");
    m_typeFilter->addItem("OVA", "OVA");
    m_typeFilter->addItem("TV Special", "TV Special");
    m_typeFilter->addItem("Web", "Web");
    m_typeFilter->addItem("Music Video", "Music Video");
    m_typeFilter->addItem("Other", "Other");
    
    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MyListFilterSidebar::onFilterChanged);
    
    typeLayout->addWidget(typeLabel);
    typeLayout->addWidget(m_typeFilter);
    mainLayout->addWidget(typeGroup);
    
    // In MyList filter group
    QGroupBox *mylistGroup = new QGroupBox("MyList");
    QVBoxLayout *mylistLayout = new QVBoxLayout(mylistGroup);
    
    m_inMyListCheckbox = new QCheckBox("In MyList only");
    m_inMyListCheckbox->setChecked(true);  // Default to showing only mylist anime
    connect(m_inMyListCheckbox, &QCheckBox::clicked,
            this, &MyListFilterSidebar::onFilterChanged);
    
    mylistLayout->addWidget(m_inMyListCheckbox);
    mainLayout->addWidget(mylistGroup);
    
    // Filter by completion status group
    QGroupBox *completionGroup = new QGroupBox("Completion");
    QVBoxLayout *completionLayout = new QVBoxLayout(completionGroup);
    
    QLabel *completionLabel = new QLabel("Status:");
    m_completionFilter = new QComboBox();
    m_completionFilter->addItem("All", "");
    m_completionFilter->addItem("Completed", "completed");
    m_completionFilter->addItem("Watching", "watching");
    m_completionFilter->addItem("Not Started", "notstarted");
    
    connect(m_completionFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MyListFilterSidebar::onFilterChanged);
    
    completionLayout->addWidget(completionLabel);
    completionLayout->addWidget(m_completionFilter);
    mainLayout->addWidget(completionGroup);
    
    // Show only unwatched checkbox
    QGroupBox *viewedGroup = new QGroupBox("Viewed Status");
    QVBoxLayout *viewedLayout = new QVBoxLayout(viewedGroup);
    
    m_showOnlyUnwatchedCheckbox = new QCheckBox("Show only with unwatched episodes");
    connect(m_showOnlyUnwatchedCheckbox, &QCheckBox::clicked,
            this, &MyListFilterSidebar::onFilterChanged);
    
    viewedLayout->addWidget(m_showOnlyUnwatchedCheckbox);
    mainLayout->addWidget(viewedGroup);
    
    // Show marked for deletion checkbox
    QGroupBox *deletionGroup = new QGroupBox("Deletion Status");
    QVBoxLayout *deletionLayout = new QVBoxLayout(deletionGroup);
    
    m_showMarkedForDeletionCheckbox = new QCheckBox("Show only marked for deletion");
    connect(m_showMarkedForDeletionCheckbox, &QCheckBox::clicked,
            this, &MyListFilterSidebar::onFilterChanged);
    
    deletionLayout->addWidget(m_showMarkedForDeletionCheckbox);
    mainLayout->addWidget(deletionGroup);
    
    // Adult content filter group
    QGroupBox *adultContentGroup = new QGroupBox("Adult Content");
    QVBoxLayout *adultContentLayout = new QVBoxLayout(adultContentGroup);
    
    QLabel *adultContentLabel = new QLabel("Filter:");
    m_adultContentFilter = new QComboBox();
    m_adultContentFilter->addItem("Ignore", "ignore");
    m_adultContentFilter->addItem("Hide 18+", "hide");
    m_adultContentFilter->addItem("Show only 18+", "showonly");
    m_adultContentFilter->setCurrentIndex(1);  // Default to "Hide 18+"
    
    connect(m_adultContentFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MyListFilterSidebar::onFilterChanged);
    
    adultContentLayout->addWidget(adultContentLabel);
    adultContentLayout->addWidget(m_adultContentFilter);
    mainLayout->addWidget(adultContentGroup);
    
    // Reset button
    m_resetButton = new QPushButton("Reset All Filters");
    connect(m_resetButton, &QPushButton::clicked, this, &MyListFilterSidebar::resetFilters);
    mainLayout->addWidget(m_resetButton);
    
    // Add stretch to push everything to the top
    mainLayout->addStretch();
    
    // Set minimum and maximum width for the sidebar
    setMinimumWidth(200);
    setMaximumWidth(300);
}

QString MyListFilterSidebar::getSearchText() const
{
    return m_searchField->text().trimmed();
}

QString MyListFilterSidebar::getTypeFilter() const
{
    return m_typeFilter->currentData().toString();
}

QString MyListFilterSidebar::getCompletionFilter() const
{
    return m_completionFilter->currentData().toString();
}

bool MyListFilterSidebar::getShowOnlyUnwatched() const
{
    return m_showOnlyUnwatchedCheckbox->isChecked();
}

bool MyListFilterSidebar::getShowMarkedForDeletion() const
{
    return m_showMarkedForDeletionCheckbox->isChecked();
}

int MyListFilterSidebar::getSortIndex() const
{
    return m_sortComboBox->currentIndex();
}

bool MyListFilterSidebar::getSortAscending() const
{
    return m_sortAscending;
}

QString MyListFilterSidebar::getAdultContentFilter() const
{
    return m_adultContentFilter->currentData().toString();
}

bool MyListFilterSidebar::getInMyListOnly() const
{
    return m_inMyListCheckbox->isChecked();
}

void MyListFilterSidebar::resetFilters()
{
    m_searchField->clear();
    m_sortComboBox->setCurrentIndex(2);  // Reset to Aired Date
    m_sortAscending = false;
    m_sortOrderButton->setText("↓ Desc");
    m_typeFilter->setCurrentIndex(0);
    m_completionFilter->setCurrentIndex(0);
    m_showOnlyUnwatchedCheckbox->setChecked(false);
    m_showMarkedForDeletionCheckbox->setChecked(false);
    m_inMyListCheckbox->setChecked(true);  // Default to showing only mylist
    m_adultContentFilter->setCurrentIndex(1);  // Reset to "Hide 18+"
    
    emit sortChanged();
    emit filterChanged();
}

void MyListFilterSidebar::onSearchTextChanged()
{
    emit filterChanged();
}

void MyListFilterSidebar::onFilterChanged()
{
    emit filterChanged();
}

void MyListFilterSidebar::onSortChanged()
{
    emit sortChanged();
}

void MyListFilterSidebar::onSortOrderToggled()
{
    m_sortAscending = !m_sortAscending;
    
    // Update button text
    if (m_sortAscending) {
        m_sortOrderButton->setText("↑ Asc");
    } else {
        m_sortOrderButton->setText("↓ Desc");
    }
    
    emit sortChanged();
}

void MyListFilterSidebar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // Position collapse button on the right side, ignoring layout
    if (m_collapseButton) {
        int buttonWidth = m_collapseButton->width();
        int buttonHeight = m_collapseButton->height();
        // Position at top-right corner with some margin
        m_collapseButton->move(width() - buttonWidth - 10, 0);
    }
}
