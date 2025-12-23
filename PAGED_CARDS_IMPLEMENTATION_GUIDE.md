# Paged Cards - Technical Implementation Guide

## Overview

This document provides a detailed technical guide for implementing paged cards functionality in the Usagi-dono anime list manager. It covers three implementation approaches with code examples and integration points.

## Approach 1: Wrapper-Based Implementation (Recommended for POC)

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    VirtualFlowLayout                        │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │            ChainCardWrapper (new)                   │   │
│  │  ┌──────────────────────────────────────────────┐  │   │
│  │  │         AnimeCard 1 (hidden)                 │  │   │
│  │  └──────────────────────────────────────────────┘  │   │
│  │  ┌──────────────────────────────────────────────┐  │   │
│  │  │         AnimeCard 2 (hidden)                 │  │   │
│  │  └──────────────────────────────────────────────┘  │   │
│  │  ┌──────────────────────────────────────────────┐  │   │
│  │  │         AnimeCard 3 (visible)                │  │   │
│  │  └──────────────────────────────────────────────┘  │   │
│  │  ┌──────────────────────────────────────────────┐  │   │
│  │  │         AnimeCard 4 (hidden)                 │  │   │
│  │  └──────────────────────────────────────────────┘  │   │
│  │                                                     │   │
│  │  [← Prev]  [Chain Info]  [Next →]                 │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### New Class: ChainCardWrapper

**File: `usagi/src/chaincardwrapper.h`**

```cpp
#ifndef CHAINCARDWRAPPER_H
#define CHAINCARDWRAPPER_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "animecard.h"
#include "animechain.h"

/**
 * ChainCardWrapper - Wraps multiple AnimeCard instances for paged navigation
 * 
 * Displays one card at a time from a chain with navigation controls.
 * All cards are kept in memory but only one is visible.
 */
class ChainCardWrapper : public QWidget
{
    Q_OBJECT
    
public:
    explicit ChainCardWrapper(const AnimeChain& chain, QWidget *parent = nullptr);
    virtual ~ChainCardWrapper();
    
    // Get the current anime ID being displayed
    int getCurrentAnimeId() const;
    
    // Get the chain this wrapper represents
    AnimeChain getChain() const { return m_chain; }
    
    // Navigate to specific anime in chain
    void navigateToAnime(int aid);
    
    // Navigate by index
    void navigateToIndex(int index);
    
    // Add a card to the wrapper
    void addCard(AnimeCard* card);
    
    // Size management (match AnimeCard size)
    QSize sizeHint() const override { return AnimeCard::getCardSize(); }
    QSize minimumSizeHint() const override { return AnimeCard::getCardSize(); }
    
signals:
    // Emitted when user navigates to different anime in chain
    void currentAnimeChanged(int aid);
    
    // Forward card signals
    void playAnimeRequested(int aid);
    void downloadAnimeRequested(int aid);
    void hideCardRequested(int aid);
    
private slots:
    void onPreviousClicked();
    void onNextClicked();
    void onChainOverviewClicked();
    void updateNavigationControls();
    
private:
    void setupUI();
    void connectCardSignals(AnimeCard* card);
    
    AnimeChain m_chain;
    QStackedWidget* m_cardStack;
    QPushButton* m_prevButton;
    QPushButton* m_nextButton;
    QLabel* m_chainInfoLabel;
    QPushButton* m_chainOverviewButton;
    
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_navLayout;
    
    int m_currentIndex;
    QMap<int, AnimeCard*> m_cards;  // aid -> card mapping
};

#endif // CHAINCARDWRAPPER_H
```

**File: `usagi/src/chaincardwrapper.cpp`**

```cpp
#include "chaincardwrapper.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyle>

ChainCardWrapper::ChainCardWrapper(const AnimeChain& chain, QWidget *parent)
    : QWidget(parent)
    , m_chain(chain)
    , m_currentIndex(0)
{
    setupUI();
}

ChainCardWrapper::~ChainCardWrapper()
{
    // Cards are children, Qt will delete them
}

void ChainCardWrapper::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Navigation header
    m_navLayout = new QHBoxLayout();
    
    m_prevButton = new QPushButton("← Previous");
    m_prevButton->setMaximumWidth(100);
    connect(m_prevButton, &QPushButton::clicked, this, &ChainCardWrapper::onPreviousClicked);
    
    m_chainInfoLabel = new QLabel();
    m_chainInfoLabel->setAlignment(Qt::AlignCenter);
    
    m_chainOverviewButton = new QPushButton("⋮");  // Vertical ellipsis
    m_chainOverviewButton->setMaximumWidth(30);
    m_chainOverviewButton->setToolTip("Show chain overview");
    connect(m_chainOverviewButton, &QPushButton::clicked, this, &ChainCardWrapper::onChainOverviewClicked);
    
    m_nextButton = new QPushButton("Next →");
    m_nextButton->setMaximumWidth(100);
    connect(m_nextButton, &QPushButton::clicked, this, &ChainCardWrapper::onNextClicked);
    
    m_navLayout->addWidget(m_prevButton);
    m_navLayout->addStretch();
    m_navLayout->addWidget(m_chainInfoLabel);
    m_navLayout->addWidget(m_chainOverviewButton);
    m_navLayout->addStretch();
    m_navLayout->addWidget(m_nextButton);
    
    m_mainLayout->addLayout(m_navLayout);
    
    // Stacked widget for cards
    m_cardStack = new QStackedWidget();
    m_mainLayout->addWidget(m_cardStack);
    
    updateNavigationControls();
}

void ChainCardWrapper::addCard(AnimeCard* card)
{
    if (!card) return;
    
    int aid = card->getAnimeId();
    m_cards[aid] = card;
    m_cardStack->addWidget(card);
    
    // Connect card signals
    connectCardSignals(card);
    
    // If this is the first card, show it
    if (m_cardStack->count() == 1) {
        m_currentIndex = 0;
        m_cardStack->setCurrentWidget(card);
        updateNavigationControls();
    }
}

void ChainCardWrapper::connectCardSignals(AnimeCard* card)
{
    connect(card, &AnimeCard::playAnimeRequested, 
            this, &ChainCardWrapper::playAnimeRequested);
    connect(card, &AnimeCard::downloadAnimeRequested, 
            this, &ChainCardWrapper::downloadAnimeRequested);
    connect(card, &AnimeCard::hideCardRequested, 
            this, &ChainCardWrapper::hideCardRequested);
}

int ChainCardWrapper::getCurrentAnimeId() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_chain.size()) {
        return m_chain.getAnimeIds()[m_currentIndex];
    }
    return 0;
}

void ChainCardWrapper::navigateToAnime(int aid)
{
    QList<int> animeIds = m_chain.getAnimeIds();
    int index = animeIds.indexOf(aid);
    if (index >= 0) {
        navigateToIndex(index);
    }
}

void ChainCardWrapper::navigateToIndex(int index)
{
    if (index < 0 || index >= m_chain.size()) {
        return;
    }
    
    m_currentIndex = index;
    
    // Get the anime ID for this index
    int aid = m_chain.getAnimeIds()[index];
    
    // Show the corresponding card
    if (m_cards.contains(aid)) {
        m_cardStack->setCurrentWidget(m_cards[aid]);
        updateNavigationControls();
        emit currentAnimeChanged(aid);
    }
}

void ChainCardWrapper::onPreviousClicked()
{
    if (m_currentIndex > 0) {
        navigateToIndex(m_currentIndex - 1);
    }
}

void ChainCardWrapper::onNextClicked()
{
    if (m_currentIndex < m_chain.size() - 1) {
        navigateToIndex(m_currentIndex + 1);
    }
}

void ChainCardWrapper::onChainOverviewClicked()
{
    // TODO: Show popup with full chain overview
    // For now, just cycle through cards quickly
}

void ChainCardWrapper::updateNavigationControls()
{
    int chainSize = m_chain.size();
    
    // Update button states
    m_prevButton->setEnabled(m_currentIndex > 0);
    m_nextButton->setEnabled(m_currentIndex < chainSize - 1);
    
    // Update info label
    if (chainSize > 1) {
        // Get chain name from first anime or use generic name
        QString chainName = "Series Chain";
        if (!m_cards.isEmpty()) {
            AnimeCard* firstCard = m_cards[m_chain.getAnimeIds().first()];
            if (firstCard) {
                // Could derive chain name from first anime title
                chainName = firstCard->getAnimeTitle().section(':', 0, 0);  // Get base title
            }
        }
        
        m_chainInfoLabel->setText(QString("%1 - Showing %2 of %3")
            .arg(chainName)
            .arg(m_currentIndex + 1)
            .arg(chainSize));
    } else {
        m_chainInfoLabel->setText("Standalone");
    }
    
    // Show/hide navigation for single-entry chains
    bool isChain = chainSize > 1;
    m_prevButton->setVisible(isChain);
    m_nextButton->setVisible(isChain);
    m_chainOverviewButton->setVisible(isChain);
}
```

### Modifications to MyListCardManager

**File: `usagi/src/mylistcardmanager.h`**

Add new method:
```cpp
// Create a wrapper card for a chain (paged mode)
ChainCardWrapper* createChainCard(const AnimeChain& chain);

// Enable/disable paged mode
void setPagedModeEnabled(bool enabled) { m_pagedModeEnabled = enabled; }
bool isPagedModeEnabled() const { return m_pagedModeEnabled; }

private:
    bool m_pagedModeEnabled;  // Add to class members
```

**File: `usagi/src/mylistcardmanager.cpp`**

Modify `setAnimeIdList`:
```cpp
void MyListCardManager::setAnimeIdList(const QList<int>& aids, bool chainModeEnabled)
{
    QList<int> finalAnimeIds;
    
    {
        QMutexLocker locker(&m_mutex);
        m_chainModeEnabled = chainModeEnabled;
        
        if (chainModeEnabled && m_chainsBuilt) {
            // Build filtered chains
            QList<AnimeChain> filteredChains;
            // ... existing chain building logic ...
            
            if (m_pagedModeEnabled) {
                // PAGED MODE: Only use representative IDs
                finalAnimeIds.clear();
                for (const AnimeChain& chain : filteredChains) {
                    // Add only the representative (first) anime ID
                    finalAnimeIds.append(chain.getRepresentativeAnimeId());
                }
                LOG(QString("[MyListCardManager] Paged mode: %1 chains (representatives only)")
                    .arg(finalAnimeIds.size()));
            } else {
                // NORMAL MODE: Use all anime IDs from chains
                // ... existing logic ...
            }
            
            m_displayedChains = filteredChains;
        } else {
            // Non-chain mode
            m_displayedChains.clear();
            finalAnimeIds = aids;
        }
        
        m_orderedAnimeIds = finalAnimeIds;
    }
    
    if (m_virtualLayout) {
        m_virtualLayout->setItemCount(finalAnimeIds.size());
    }
}
```

Add `createChainCard` method:
```cpp
ChainCardWrapper* MyListCardManager::createChainCard(const AnimeChain& chain)
{
    ChainCardWrapper* wrapper = new ChainCardWrapper(chain);
    
    // Create cards for all anime in the chain
    for (int aid : chain.getAnimeIds()) {
        AnimeCard* card = createCard(aid);
        if (card) {
            wrapper->addCard(card);
        }
    }
    
    // Connect wrapper signals
    connect(wrapper, &ChainCardWrapper::playAnimeRequested,
            this, [this](int aid) {
                // Forward to appropriate handler
                emit /* appropriate signal */;
            });
    
    return wrapper;
}
```

Modify `createCardForIndex`:
```cpp
AnimeCard* MyListCardManager::createCardForIndex(int index)
{
    QMutexLocker locker(&m_mutex);
    
    // Wait for data to be ready
    while (!m_dataReady) {
        m_dataReadyCondition.wait(&m_mutex);
    }
    
    if (index < 0 || index >= m_orderedAnimeIds.size()) {
        return nullptr;
    }
    
    int aid = m_orderedAnimeIds[index];
    
    if (m_pagedModeEnabled && m_chainModeEnabled) {
        // Create chain wrapper instead of single card
        AnimeChain chain = getChainForAnime(aid);
        return createChainCard(chain);  // Returns QWidget*, upcast to AnimeCard*
                                        // NOTE: This needs type adjustment
    } else {
        // Normal mode: create single card
        return createCard(aid);
    }
}
```

**Note**: The above approach has a type mismatch issue. `ChainCardWrapper` is not an `AnimeCard`. Solutions:

1. Change return type to `QWidget*`
2. Make `ChainCardWrapper` inherit from `AnimeCard` (not clean)
3. Use a common interface/base class

### VirtualFlowLayout Adjustments

**File: `usagi/src/virtualflowlayout.h`**

Change factory signature:
```cpp
// Old:
using ItemFactory = std::function<QWidget*(int index)>;

// Keep same (works for both AnimeCard and ChainCardWrapper)
```

No changes needed! `VirtualFlowLayout` already works with `QWidget*`.

### Filter Sidebar Addition

**File: `usagi/src/mylistfiltersidebar.h`**

Add checkbox:
```cpp
QCheckBox *m_showSeriesChainCheckbox;     // Existing
QCheckBox *m_usePagedChainCheckbox;       // New
```

**File: `usagi/src/mylistfiltersidebar.cpp`**

Add checkbox to UI:
```cpp
m_usePagedChainCheckbox = new QCheckBox("Use paged chain display");
m_usePagedChainCheckbox->setToolTip("Show one card per chain with navigation arrows");
m_usePagedChainCheckbox->setEnabled(false);  // Disabled by default
connect(m_usePagedChainCheckbox, &QCheckBox::clicked,
        this, &MyListFilterSidebar::onFilterChanged);

// Enable only when series chain is enabled
connect(m_showSeriesChainCheckbox, &QCheckBox::toggled, [this](bool checked) {
    m_usePagedChainCheckbox->setEnabled(checked);
    if (!checked) {
        m_usePagedChainCheckbox->setChecked(false);
    }
});

seriesChainLayout->addWidget(m_showSeriesChainCheckbox);
seriesChainLayout->addWidget(m_usePagedChainCheckbox);
```

## Approach 2: Native AnimeCard Enhancement

### Modified AnimeCard

**File: `usagi/src/animecard.h`**

Add chain support:
```cpp
class AnimeCard : public QFrame
{
    Q_OBJECT
    
public:
    // ... existing methods ...
    
    // Chain support
    void setChain(const AnimeChain& chain);
    void setDisplayMode(bool pagedMode);
    void showAnimeInChain(int index);
    
signals:
    void chainNavigationRequested(int aid);
    
private:
    bool m_isChainCard;
    AnimeChain m_chain;
    int m_chainCurrentIndex;
    
    QPushButton* m_chainPrevButton;
    QPushButton* m_chainNextButton;
    QLabel* m_chainInfoLabel;
    
    void setupChainNavigation();
    void updateChainNavigation();
    void loadAnimeData(int aid);  // Reload card with different anime
};
```

This approach requires extensive refactoring of `AnimeCard` and is more invasive.

## Approach 3: Hybrid - Two Card Types

### Create PagedAnimeCard

**File: `usagi/src/pagedanimecard.h`**

```cpp
#ifndef PAGEDANIMECARD_H
#define PAGEDANIMECARD_H

#include "animecard.h"
#include "animechain.h"

class PagedAnimeCard : public AnimeCard
{
    Q_OBJECT
    
public:
    explicit PagedAnimeCard(const AnimeChain& chain, QWidget *parent = nullptr);
    
    // Override display to show current anime in chain
    void showAnime(int aid);
    
private:
    AnimeChain m_chain;
    int m_currentIndex;
    // ... navigation controls ...
};

#endif
```

## Integration with Window

**File: `usagi/src/window.cpp`**

Connect paged mode checkbox:
```cpp
void Window::onMyListFilterChanged()
{
    bool chainMode = m_filterSidebar->getShowSeriesChain();
    bool pagedMode = m_filterSidebar->getUsePagedChain();  // New getter
    
    m_cardManager->setPagedModeEnabled(pagedMode);
    
    applyMylistFilters();
}
```

## Testing Strategy

### Unit Tests

Create `tests/test_chaincardwrapper.cpp`:
```cpp
#include <QtTest>
#include "chaincardwrapper.h"
#include "animechain.h"

class TestChainCardWrapper : public QObject
{
    Q_OBJECT
    
private slots:
    void testNavigation();
    void testCardVisibility();
    void testSignalForwarding();
};

void TestChainCardWrapper::testNavigation()
{
    // Create test chain
    AnimeChain chain({144, 6716, 15546});
    ChainCardWrapper wrapper(chain);
    
    // Add test cards
    // ...
    
    // Test navigation
    QCOMPARE(wrapper.getCurrentAnimeId(), 144);
    wrapper.navigateToIndex(1);
    QCOMPARE(wrapper.getCurrentAnimeId(), 6716);
}
```

### Manual Testing

1. Enable "Display series chain" checkbox
2. Enable "Use paged chain display" checkbox
3. Verify chains show as single cards with navigation
4. Test navigation with mouse clicks
5. Test navigation with keyboard (if implemented)
6. Verify episode playback works correctly
7. Test with single-entry chains (no navigation)
8. Test with long chains (10+ entries)

## Performance Considerations

### Memory Usage

**Current mode**: N anime → N cards in memory → N visible widgets
**Paged mode**: N anime → N cards in memory → 1 visible widget per chain

For a chain of 4 anime:
- Current: 4 cards rendered, 4 visible
- Paged: 4 cards created, 1 visible at a time

Memory savings: Minimal (all cards still created)
Rendering savings: Significant (fewer visible widgets)

### Optimization: Lazy Card Creation

Instead of creating all cards upfront:
```cpp
void ChainCardWrapper::navigateToIndex(int index)
{
    int aid = m_chain.getAnimeIds()[index];
    
    // Lazy creation
    if (!m_cards.contains(aid)) {
        AnimeCard* card = m_cardManager->createCard(aid);
        addCard(card);
    }
    
    m_cardStack->setCurrentWidget(m_cards[aid]);
}
```

This reduces memory usage but increases navigation latency.

## Migration Path

### Phase 1: POC (Week 1)
- Implement `ChainCardWrapper`
- Add checkbox to filter sidebar
- Basic navigation (previous/next)
- Test with small datasets

### Phase 2: Polish (Week 2)
- Add keyboard shortcuts
- Add chain overview popup
- Improve visual design
- Animation/transitions

### Phase 3: Optimization (Week 3)
- Lazy card creation
- Touch gesture support
- Performance profiling
- Bug fixes

### Phase 4: Production (Week 4)
- User testing
- Documentation
- Settings persistence
- Release

## Rollback Plan

If paged mode causes issues:
1. Disable checkbox by default
2. Add setting to hide feature entirely
3. Can be toggled via config file
4. Future: Remove code if unused

## Code Organization

```
usagi/src/
  ├── chaincardwrapper.h         (new)
  ├── chaincardwrapper.cpp       (new)
  ├── mylistcardmanager.h        (modified)
  ├── mylistcardmanager.cpp      (modified)
  ├── mylistfiltersidebar.h      (modified)
  ├── mylistfiltersidebar.cpp    (modified)
  ├── window.cpp                 (modified)
  └── animecard.h/cpp            (unchanged)

tests/
  └── test_chaincardwrapper.cpp  (new)
```

## Summary

This implementation guide provides three approaches for paged cards:

1. **Wrapper approach**: Clean separation, minimal changes, best for POC
2. **Native enhancement**: More integrated, requires refactoring
3. **Hybrid approach**: Two card types, flexibility at cost of duplication

**Recommendation**: Start with Approach 1 (Wrapper) for proof of concept, then evaluate user feedback before committing to Approach 2 for production.
