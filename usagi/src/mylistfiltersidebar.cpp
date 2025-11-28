#include "mylistfiltersidebar.h"
#include <QLabel>
#include <QTimer>

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
    
    // Title
    QLabel *titleLabel = new QLabel("<b>Search & Filter</b>");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
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
    
    // Session settings group
    QGroupBox *sessionGroup = new QGroupBox("Session Settings");
    QVBoxLayout *sessionLayout = new QVBoxLayout(sessionGroup);
    
    // Ahead buffer setting - global value that applies to all anime
    QLabel *aheadBufferLabel = new QLabel("Episodes ahead:");
    m_aheadBufferSpinBox = new QSpinBox();
    m_aheadBufferSpinBox->setMinimum(1);
    m_aheadBufferSpinBox->setMaximum(20);
    m_aheadBufferSpinBox->setValue(3);
    m_aheadBufferSpinBox->setToolTip("Number of episodes to keep ready for uninterrupted viewing.\n"
                                      "This value applies to all anime with active sessions.");
    connect(m_aheadBufferSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MyListFilterSidebar::onSessionSettingsChanged);
    
    QHBoxLayout *aheadLayout = new QHBoxLayout();
    aheadLayout->addWidget(aheadBufferLabel);
    aheadLayout->addWidget(m_aheadBufferSpinBox);
    sessionLayout->addLayout(aheadLayout);
    
    // Deletion threshold type
    QLabel *thresholdTypeLabel = new QLabel("Deletion threshold:");
    m_thresholdTypeComboBox = new QComboBox();
    m_thresholdTypeComboBox->addItem("Fixed (GB)", 0);
    m_thresholdTypeComboBox->addItem("Percentage (%)", 1);
    m_thresholdTypeComboBox->setToolTip("Type of threshold for automatic file cleanup");
    connect(m_thresholdTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MyListFilterSidebar::onSessionSettingsChanged);
    
    sessionLayout->addWidget(thresholdTypeLabel);
    sessionLayout->addWidget(m_thresholdTypeComboBox);
    
    // Deletion threshold value
    QLabel *thresholdValueLabel = new QLabel("Threshold value:");
    m_thresholdValueSpinBox = new QDoubleSpinBox();
    m_thresholdValueSpinBox->setMinimum(1.0);
    m_thresholdValueSpinBox->setMaximum(1000.0);
    m_thresholdValueSpinBox->setValue(50.0);
    m_thresholdValueSpinBox->setSuffix(" GB");
    m_thresholdValueSpinBox->setToolTip("When free space drops below this value, files will be marked for deletion");
    connect(m_thresholdValueSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MyListFilterSidebar::onSessionSettingsChanged);
    
    QHBoxLayout *thresholdLayout = new QHBoxLayout();
    thresholdLayout->addWidget(thresholdValueLabel);
    thresholdLayout->addWidget(m_thresholdValueSpinBox);
    sessionLayout->addLayout(thresholdLayout);
    
    // Auto-mark deletion checkbox
    m_autoMarkDeletionCheckbox = new QCheckBox("Auto-mark for deletion");
    m_autoMarkDeletionCheckbox->setToolTip("Automatically mark watched files for deletion when disk space is low");
    connect(m_autoMarkDeletionCheckbox, &QCheckBox::clicked,
            this, &MyListFilterSidebar::onSessionSettingsChanged);
    
    sessionLayout->addWidget(m_autoMarkDeletionCheckbox);
    mainLayout->addWidget(sessionGroup);
    
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

void MyListFilterSidebar::onSessionSettingsChanged()
{
    // Update threshold suffix based on type
    if (m_thresholdTypeComboBox->currentData().toInt() == 0) {
        m_thresholdValueSpinBox->setSuffix(" GB");
    } else {
        m_thresholdValueSpinBox->setSuffix(" %");
    }
    
    emit sessionSettingsChanged();
}

// Session settings getters
int MyListFilterSidebar::getAheadBuffer() const
{
    return m_aheadBufferSpinBox->value();
}

int MyListFilterSidebar::getDeletionThresholdType() const
{
    return m_thresholdTypeComboBox->currentData().toInt();
}

double MyListFilterSidebar::getDeletionThresholdValue() const
{
    return m_thresholdValueSpinBox->value();
}

bool MyListFilterSidebar::isAutoMarkDeletionEnabled() const
{
    return m_autoMarkDeletionCheckbox->isChecked();
}

// Session settings setters
void MyListFilterSidebar::setAheadBuffer(int episodes)
{
    m_aheadBufferSpinBox->setValue(episodes);
}

void MyListFilterSidebar::setDeletionThresholdType(int type)
{
    m_thresholdTypeComboBox->setCurrentIndex(type);
    if (type == 0) {
        m_thresholdValueSpinBox->setSuffix(" GB");
    } else {
        m_thresholdValueSpinBox->setSuffix(" %");
    }
}

void MyListFilterSidebar::setDeletionThresholdValue(double value)
{
    m_thresholdValueSpinBox->setValue(value);
}

void MyListFilterSidebar::setAutoMarkDeletionEnabled(bool enabled)
{
    m_autoMarkDeletionCheckbox->setChecked(enabled);
}
