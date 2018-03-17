
#include "Hanabi.h"

struct CardKnowledge {
    CardKnowledge();

    bool mustBe(Hanabi::Color color) const;
    bool mustBe(Hanabi::Value value) const;
    bool cannotBe(Hanabi::Color color) const;
    bool cannotBe(Hanabi::Value value) const;
    int value() const;

    void setMustBe(Hanabi::Color color);
    void setMustBe(Hanabi::Value value);

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

class ValueBot final : public Hanabi::Bot {

    int me_;
    int myHandSize_;  /* purely for convenience */

    /* What does each player know about his own hand? */
    std::vector<std::vector<CardKnowledge> > handKnowledge_;
    /* What cards have been seen so far? */
    int cardCount_[Hanabi::NUMCOLORS][5+1];

    /* Returns the lowest value of card that is currently playable. */
    int lowestPlayableValue() const;

    bool couldBeValuable(int value) const;

    void invalidateKnol(int player_index, int card_index);
    void seePublicCard(const Hanabi::Card &played_card);
    void wipeOutPlayables(const Hanabi::Server &server, const Hanabi::Card &played_card);
    void makeThisValueWorthless(const Hanabi::Server &server, Hanabi::Value value);

    /* Returns -1 if the named player is planning to play a card on his
     * turn, or if all his cards are known to be valuable. Otherwise,
     * returns the index of his oldest not-known-valuable card. */
    int nextDiscardIndex(const Hanabi::Server &server, int player) const;

    Hint bestHintForPlayer(const Hanabi::Server &server, int to) const;

    bool maybePlayLowestPlayableCard(Hanabi::Server &server);
    bool maybeGiveHelpfulHint(Hanabi::Server &server);
    bool maybeGiveValuableWarning(Hanabi::Server &server);
    bool maybeDiscardWorthlessCard(Hanabi::Server &server);
    bool maybeDiscardOldCard(Hanabi::Server &server);

  public:
    ValueBot(int index, int numPlayers, int handSize);
    void pleaseObserveBeforeMove(const Hanabi::Server &) override;
    void pleaseMakeMove(Hanabi::Server &) override;
      void pleaseObserveBeforeDiscard(const Hanabi::Server &, int from, int card_index) override;
      void pleaseObserveBeforePlay(const Hanabi::Server &, int from, int card_index) override;
      void pleaseObserveColorHint(const Hanabi::Server &, int from, int to, Hanabi::Color color, Hanabi::CardIndices card_indices) override;
      void pleaseObserveValueHint(const Hanabi::Server &, int from, int to, Hanabi::Value value, Hanabi::CardIndices card_indices) override;
    void pleaseObserveAfterMove(const Hanabi::Server &) override;
};
