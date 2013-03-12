
#include "Hanabi.h"

struct CardKnowledge {
    CardKnowledge();

    bool mustBe(Hanabi::Color color) const;
    bool mustBe(Hanabi::Value value) const;

    void setMustBe(Hanabi::Color color);
    void setMustBe(Hanabi::Value value);

    bool isPlayable;

private:
    enum Possibility { NO, MAYBE, YES };
    Possibility colors_[Hanabi::NUMCOLORS];
    Possibility values_[5+1];
};

class SimpleBot : public Hanabi::Bot {

    int me_;

    /* What does each player know about his own hand? */
    std::vector<std::vector<CardKnowledge> > handKnowledge_;

    /* Sneaky tricks to deal with the lack of notifications about our own play. */
    bool needsToPostObserve_;
    Hanabi::Pile postObservedPiles_[Hanabi::NUMCOLORS];

    void invalidateKnol(int player_index, int card_index);
    void prepareToPostObserve(const Hanabi::Server &server);
    void wipeOutPlayables(const Hanabi::Card &played_card);

  public:
    SimpleBot(int index, int numPlayers);
    virtual void pleaseObserveBeforeMove(const Hanabi::Server &);
    virtual void pleaseMakeMove(Hanabi::Server &);
      virtual void pleaseObserveBeforeDiscard(const Hanabi::Server &, int from, int card_index);
      virtual void pleaseObserveBeforePlay(const Hanabi::Server &, int from, int card_index);
      virtual void pleaseObserveColorHint(const Hanabi::Server &, int from, int to, Hanabi::Color color, std::vector<int> card_indices);
      virtual void pleaseObserveValueHint(const Hanabi::Server &, int from, int to, Hanabi::Value value, std::vector<int> card_indices);
    virtual void pleaseObserveAfterMove(const Hanabi::Server &);
};
