#include "mylistfiltersidebar.h"
#include <QLabel>
#include <QTimer>

MyListFilterSidebar::MyListFilterSidebar(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

MyListFilterSidebar::~MyListFilterSidebar()
{
}

void MyListFilterSidebar::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(10);
    
    // Title
    auto *titleLabel = new QLabel("<b>Search & Filter</b>");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // Search group
    auto *searchGroup = new QGroupBox("Search");
    auto *searchLayout = new QVBoxLayout(searchGroup);
    
    auto *searchLabel = new QLabel("Anime Title:");
    m_searchField = new QLineEdit();
    m_searchField->setPlaceholderText("Search by title or alternative title...");
    m_searchField->setClearButtonEnabled(true);
    
    // Use a timer to debounce search input (avoid filtering on every keystroke)
    auto *searchDebounceTimer = new QTimer(this);
    searchDebounceTimer->setSingleShot(true);
    searchDebounceTimer->setInterval(300); // 300ms delay
    
    connect(m_searchField, &QLineEdit::textChanged, this, [this, searchDebounceTimer]() {
        searchDebounceTimer->start();
    });
    connect(searchDebounceTimer, &QTimer::timeout, this, &MyListFilterSidebar::onSearchTextChanged);
    
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchField);
    mainLayout->addWidget(searchGroup);
    
    // Filter by type group
    auto *typeGroup = new QGroupBox("Type");
    auto *typeLayout = new QVBoxLayout(typeGroup);
    
    auto *typeLabel = new QLabel("Anime Type:");
    m_typeFilter = new QComboBox();
    m_typeFilter->addItem("All Types", "");
    m_typeFilter->addItem("TV Series", "TV Series");
    m_typeFilter->addItem("Movie", "Movie");
    m_typeFilter->addItem("OVA", "OVA");
    m_typeFilter->addItem("ONA", "ONA");
    m_typeFilter->addItem("TV Special", "TV Special");
    m_typeFilter->addItem("Web", "Web");
    m_typeFilter->addItem("Music Video", "Music Video");
    m_typeFilter->addItem("Other", "Other");
    
    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MyListFilterSidebar::onFilterChanged);
    
    typeLayout->addWidget(typeLabel);
    typeLayout->addWidget(m_typeFilter);
    mainLayout->addWidget(typeGroup);
    
    // Filter by completion status group
    auto *completionGroup = new QGroupBox("Completion");
    auto *completionLayout = new QVBoxLayout(completionGroup);
    
    auto *completionLabel = new QLabel("Status:");
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
    auto *viewedGroup = new QGroupBox("Viewed Status");
    auto *viewedLayout = new QVBoxLayout(viewedGroup);
    
    m_showOnlyUnwatchedCheckbox = new QCheckBox("Show only with unwatched episodes");
    connect(m_showOnlyUnwatchedCheckbox, &QCheckBox::stateChanged,
            this, &MyListFilterSidebar::onFilterChanged);
    
    viewedLayout->addWidget(m_showOnlyUnwatchedCheckbox);
    mainLayout->addWidget(viewedGroup);
    
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

void MyListFilterSidebar::resetFilters()
{
    m_searchField->clear();
    m_typeFilter->setCurrentIndex(0);
    m_completionFilter->setCurrentIndex(0);
    m_showOnlyUnwatchedCheckbox->setChecked(false);
    
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
