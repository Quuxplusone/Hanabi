
#include "Hanabi.h"

class HolmesBot;

struct CardKnowledge {
    CardKnowledge();

    bool mustBe(Hanabi::Color color) const;
    bool mustBe(Hanabi::Value value) const;
    bool cannotBe(Hanabi::Color color) const;
    bool cannotBe(Hanabi::Value value) const;
    int color() const;
    int value() const;

    void setMustBe(Hanabi::Color color);
    void setMustBe(Hanabi::Value value);
    void update(const Hanabi::Server &server, const HolmesBot &bot);

    bool isPlayable;
    bool isValuable;
    bool isWorthless;

private:
    enum Possibility { NO, MAYBE, YES };
    Possibility colors_[Hanabi::NUMCOLORS];
    Possibility values_[5+1];
};

struct Hint {
    int information_content;
    int to;
    int color;
    int value;

    Hint();
    void give(Hanabi::Server &);
};

class HolmesBot : public Hanabi::Bot {

    friend class CardKnowledge;

    int me_;

    /* What does each player know about his own hand? */
    std::vector<std::vector<CardKnowledge> > handKnowledge_;
    /* What cards have been played so far? */
    int playedCount_[Hanabi::NUMCOLORS][5+1];
    /* What cards in players' hands are definitely identified?
     * This table is recomputed every turn. */
    int locatedCount_[Hanabi::NUMCOLORS][5+1];

    /* Returns the lowest value of card that is currently playable. */
    Hanabi::Value lowestPlayableValue() const;

    bool couldBeValuable(int value) const;

    bool updateLocatedCount();
    void invalidateKnol(int player_index, int card_index);
    void seePublicCard(const Hanabi::Card &played_card);
    void wipeOutPlayables(const Hanabi::Card &played_card);
    void makeThisValueWorthless(Hanabi::Value value);

    /* Returns -1 if the named player is planning to play a card on his
     * turn, or if all his cards are known to be valuable. Otherwise,
     * returns the index of his oldest not-known-valuable card. */
    int nextDiscardIndex(int player) const;

    Hint bestHintForPlayer(const Hanabi::Server &server, int to) const;

    bool maybePlayLowestPlayableCard(Hanabi::Server &server);
    bool maybeGiveHelpfulHint(Hanabi::Server &server);
    bool maybeGiveValuableWarning(Hanabi::Server &server);
    bool maybeDiscardWorthlessCard(Hanabi::Server &server);
    bool maybeDiscardOldCard(Hanabi::Server &server);

  public:
    HolmesBot(int index, int numPlayers);
    virtual void pleaseObserveBeforeMove(const Hanabi::Server &);
    virtual void pleaseMakeMove(Hanabi::Server &);
      virtual void pleaseObserveBeforeDiscard(const Hanabi::Server &, int from, int card_index);
      virtual void pleaseObserveBeforePlay(const Hanabi::Server &, int from, int card_index);
      virtual void pleaseObserveColorHint(const Hanabi::Server &, int from, int to, Hanabi::Color color, const std::vector<int> &card_indices);
      virtual void pleaseObserveValueHint(const Hanabi::Server &, int from, int to, Hanabi::Value value, const std::vector<int> &card_indices);
    virtual void pleaseObserveAfterMove(const Hanabi::Server &);
};
