
#include "Hanabi.h"

class CrazyBot;

enum trivalue {
    NO, MAYBE, YES
};

enum issues {
    NONE,
    NEXTCHOP,
    NEXTTWOCHOP,
    AFTERNEXTCHOP
};
    

struct CardKnowledge {
    CardKnowledge();

    bool mustBe(Hanabi::Color color) const;
    bool mustBe(Hanabi::Value value) const;
    bool cannotBe(Hanabi::Card card) const;
    bool cannotBe(Hanabi::Color color) const;
    bool cannotBe(Hanabi::Value value) const;
    int color() const;
    int value() const;
    bool clued() const;

    void setClued(bool gotClued);
    int setMustBe(Hanabi::Color color);
    int setMustBe(Hanabi::Value value);
    int setCannotBe(Hanabi::Color color);
    int setCannotBe(Hanabi::Value value);
    void setIsPlayable(const Hanabi::Server &server, bool knownPlayable);
    void setIsValuable(const CrazyBot &bot, const Hanabi::Server &server, bool knownValuable);
    void setIsWorthless(const CrazyBot &bot, const Hanabi::Server &server, bool knownWorthless);
    void infer(const Hanabi::Server &server, const CrazyBot &bot);
    void update(const Hanabi::Server &server, const CrazyBot &bot, bool useMyEyesight);

    trivalue playable() const { return playable_; }
    trivalue valuable() const { return valuable_; }
    trivalue worthless() const { return worthless_; }

private:
    bool cantBe_[Hanabi::NUMCOLORS][5+1];
    int color_;
    int value_;
    trivalue playable_;
    trivalue valuable_;
    trivalue worthless_;
    bool clued_;
};

struct Hint {
    int fitness;
    int to;
    int color;
    int value;

    Hint();
    void give(Hanabi::Server &);
};

class CrazyBot : public Hanabi::Bot {

    friend class CardKnowledge;

    int me_;
    int myHandSize_;  /* purely for convenience */

    /* What does each player know about his own hand? */
    std::vector<std::vector<CardKnowledge> > handKnowledge_;
    // handLocked indicates the player must either play the handLockedIndex_ (if not -1)
    // or clue or discard
    bool handLocked_[5];
    int handLockedIndex_[5];
    // clueWaiting_ indicates that after the previous player has played (or discarded/clued)
    // the indicated clueWaitingIndex should be marked as Playable.
    bool clueWaiting_[5];
    int clueWaitingIndex_[5];
    /* What cards have been played so far? */
    int playedCount_[Hanabi::NUMCOLORS][5+1];
    /* What cards in players' hands are definitely identified?
     * This table is recomputed every turn. */
    int locatedCount_[Hanabi::NUMCOLORS][5+1];
    /* What cards in players' hands are visible to me in particular?
     * This table is recomputed every turn. */
    int eyesightCount_[Hanabi::NUMCOLORS][5+1];
    /* What is the lowest-value card currently playable?
     * This value is recomputed every turn. */
    int lowestPlayableValue_;

    bool isPlayable(const Hanabi::Server &server, Hanabi::Card card) const;
    bool isValuable(const Hanabi::Server &server, Hanabi::Card card) const;
    bool isWorthless(const Hanabi::Server &server, Hanabi::Card card) const;
    bool couldBePlayableWithValue(const Hanabi::Server &server, const CardKnowledge &knol, int value) const;
    bool couldBeValuableWithValue(const Hanabi::Server &server, const CardKnowledge &knol, int value) const;

    void updateEyesightCount(const Hanabi::Server &server);
    bool updateLocatedCount(const Hanabi::Server &server);
    void invalidateKnol(int player_index, int card_index);
    void seePublicCard(const Hanabi::Card &played_card);

    /* Returns -1 if the named player is planning to play a card on his
     * turn, or if all his cards are known to be valuable. Otherwise,
     * returns the index of his oldest not-known-valuable card. */
    int nextDiscardIndex(const Hanabi::Server &server, int player) const;
    trivalue isCluedElsewhere(const Hanabi::Server &server, int partner, int cardNum) const;

    void noValuableWarningWasGiven(const Hanabi::Server &server, int from);

    Hint bestHintForPlayer(const Hanabi::Server &server, int to) const;
    int whatFinessePlay(const Hanabi::Server &server, int partner, Hanabi::Card fcard) const;

    int lockCardToPlay(const Hanabi::Server &server, int player) const;
    int bestCardToPlay(Hanabi::Server &server);
    bool maybePlayLowestPlayableCard(Hanabi::Server &server);
    bool maybeGiveHelpfulHint(Hanabi::Server &server);
    bool maybeFinesseNextPlayer(Hanabi::Server &server);
    bool maybeGiveValuableWarning(Hanabi::Server &server, int playerDistance);
    bool maybeGiveSuperHint(Hanabi::Server &server);
    bool maybePlayMysteryCard(Hanabi::Server &server);
    bool maybeDiscardWorthlessCard(Hanabi::Server &server);
    bool maybeDiscardOldCard(Hanabi::Server &server);
    int cardOnChop(const Hanabi::Server &server, int to) const;
    int upcomingIssues(Hanabi::Server &server);

  public:
    CrazyBot(int index, int numPlayers, int handSize);
    virtual void pleaseObserveBeforeMove(const Hanabi::Server &);
    virtual void pleaseMakeMove(Hanabi::Server &);
      virtual void pleaseObserveBeforeDiscard(const Hanabi::Server &, int from, int card_index);
      virtual void pleaseObserveBeforePlay(const Hanabi::Server &, int from, int card_index);
      virtual void pleaseObserveColorHint(const Hanabi::Server &, int from, int to, Hanabi::Color color, const std::vector<int> &card_indices);
      virtual void pleaseObserveValueHint(const Hanabi::Server &, int from, int to, Hanabi::Value value, const std::vector<int> &card_indices);
    virtual void pleaseObserveAfterMove(const Hanabi::Server &);
};

// $Log: CrazyBot.h,v $
//
