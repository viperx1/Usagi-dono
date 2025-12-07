#include "animechain.h"
#include <QLabel>

AnimeChain::AnimeChain(QWidget *parent)
    : QFrame(parent)
    , m_layout(nullptr)
{
    setupUI();
}

AnimeChain::~AnimeChain()
{
    clear();
}

void AnimeChain::setupUI()
{
    // Set frame style for visual grouping
    setFrameShape(QFrame::Box);
    setFrameShadow(QFrame::Raised);
    setLineWidth(2);
    
    // Create horizontal layout for cards
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(10, 10, 10, 10);
    m_layout->setSpacing(15);  // Space between cards and arrows
    
    setLayout(m_layout);
}

void AnimeChain::addCard(AnimeCard *card)
{
    if (!card) {
        return;
    }
    
    // If this is not the first card, add an arrow before it
    if (!m_cards.isEmpty()) {
        QLabel *arrow = new QLabel("→", this);
        QFont arrowFont = arrow->font();
        arrowFont.setPointSize(24);
        arrowFont.setBold(true);
        arrow->setFont(arrowFont);
        arrow->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(arrow);
    }
    
    // Add the card
    m_layout->addWidget(card);
    m_cards.append(card);
    m_animeIds.append(card->getAnimeId());
    
    // Connect card signals to chain signals
    connectCardSignals(card);
}

void AnimeChain::clear()
{
    // Note: Cards are owned by their parent (usually the card manager)
    // We just remove them from our layout without deleting them
    while (m_layout->count() > 0) {
        QLayoutItem *item = m_layout->takeAt(0);
        if (item->widget()) {
            item->widget()->setParent(nullptr);
        }
        delete item;
    }
    
    m_cards.clear();
    m_animeIds.clear();
}

int AnimeChain::getRepresentativeAnimeId() const
{
    return m_animeIds.isEmpty() ? 0 : m_animeIds.first();
}

void AnimeChain::connectCardSignals(AnimeCard *card)
{
    connect(card, &AnimeCard::episodeClicked, this, &AnimeChain::episodeClicked);
    connect(card, &AnimeCard::cardClicked, this, &AnimeChain::cardClicked);
    connect(card, &AnimeCard::fetchDataRequested, this, &AnimeChain::fetchDataRequested);
    connect(card, &AnimeCard::playAnimeRequested, this, &AnimeChain::playAnimeRequested);
}

QSize AnimeChain::sizeHint() const
{
    if (m_cards.isEmpty()) {
        return QSize(600, 450);
    }
    
    // Calculate total width: sum of card widths + arrows + spacing
    int totalWidth = 0;
    int maxHeight = 0;
    
    for (int i = 0; i < m_cards.size(); ++i) {
        QSize cardSize = m_cards[i]->sizeHint();
        totalWidth += cardSize.width();
        maxHeight = qMax(maxHeight, cardSize.height());
        
        // Add space for arrow (except after last card)
        if (i < m_cards.size() - 1) {
            totalWidth += 50;  // Arrow width + spacing
        }
    }
    
    // Add margins
    totalWidth += m_layout->contentsMargins().left() + m_layout->contentsMargins().right();
    maxHeight += m_layout->contentsMargins().top() + m_layout->contentsMargins().bottom();
    
    return QSize(totalWidth, maxHeight);
}

QSize AnimeChain::minimumSizeHint() const
{
    return sizeHint();
}
