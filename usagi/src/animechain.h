#ifndef ANIMECHAIN_H
#define ANIMECHAIN_H

#include <QWidget>
#include <QHBoxLayout>
#include <QFrame>
#include <QList>
#include "animecard.h"

/**
 * AnimeChain - A widget that displays a series of related anime as a single chain unit
 * 
 * This widget groups related anime (prequels/sequels) into a single visual element.
 * The chain maintains internal ordering (prequel -> sequel) while allowing
 * external sorting of chains relative to each other.
 * 
 * Layout:
 * +----------------------------------------------------+
 * | [Card A] -> [Card B] -> [Card C] ...             |
 * +----------------------------------------------------+
 * 
 * Design principles:
 * - Each chain contains 1 or more anime cards
 * - Cards within a chain are ordered from prequel to sequel
 * - Chains can be sorted externally by various criteria
 * - The first anime in the chain is used as the representative for sorting
 */
class AnimeChain : public QFrame
{
    Q_OBJECT
    
public:
    explicit AnimeChain(QWidget *parent = nullptr);
    virtual ~AnimeChain();
    
    // Get the list of anime IDs in this chain (ordered from prequel to sequel)
    QList<int> getAnimeIds() const { return m_animeIds; }
    
    // Get the first (representative) anime ID for sorting
    int getRepresentativeAnimeId() const;
    
    // Add an anime card to this chain
    void addCard(AnimeCard *card);
    
    // Clear all cards from this chain
    void clear();
    
    // Get all cards in this chain
    QList<AnimeCard*> getCards() const { return m_cards; }
    
    // Get card count
    int getCardCount() const { return m_cards.size(); }
    
    // Size management
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    
signals:
    void episodeClicked(int lid);
    void cardClicked(int aid);
    void fetchDataRequested(int aid);
    void playAnimeRequested(int aid);
    
private:
    QHBoxLayout *m_layout;
    QList<AnimeCard*> m_cards;
    QList<int> m_animeIds;
    
    void setupUI();
    void connectCardSignals(AnimeCard *card);
};

#endif // ANIMECHAIN_H
