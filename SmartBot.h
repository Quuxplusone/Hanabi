
#include "Hanabi.h"

class SmartBot;

enum trivalue {
    NO, MAYBE, YES
};

class CardKnowledge {
public:
    CardKnowledge();

    bool mustBe(Hanabi::Color color) const;
    bool mustBe(Hanabi::Value value) const;
    bool cannotBe(Hanabi::Card card) const;
    bool cannotBe(Hanabi::Color color) const;
    bool cannotBe(Hanabi::Value value) const;
    int color() const { computeIdentity(); return color_; }
    int value() const { computeIdentity(); return value_; }

    void setMustBe(Hanabi::Color color);
    void setMustBe(Hanabi::Value value);
    void setCannotBe(Hanabi::Color color);
    void setCannotBe(Hanabi::Value value);
    void setIsPlayable(const Hanabi::Server &server, bool knownPlayable);
    void setIsValuable(const SmartBot &bot, const Hanabi::Server &server, bool knownValuable);
    void setIsWorthless(const SmartBot &bot, const Hanabi::Server &server, bool knownWorthless);
    void befuddleByDiscard();
    void befuddleByPlay(bool success);

    template<bool useMyEyesight>
    void update();

    bool known() const { computeIdentity(); return color_ != -1 && value_ != -1; }
    Hanabi::Card knownCard() const { return Hanabi::Card(Hanabi::Color(color_), value_); }

    trivalue playable() const { computePlayable(); return playable_; }
    trivalue valuable() const { computeValuable(); return valuable_; }
    trivalue worthless() const { computeWorthless(); return worthless_; }

    float probabilityPlayable() const { computePlayable(); return probabilityPlayable_; }
    float probabilityValuable() const { computeValuable(); return probabilityValuable_; }
    float probabilityWorthless() const { computeWorthless(); return probabilityWorthless_; }

    void computeIdentity() const;
    void computePlayable() const;
    void computeValuable() const;
    void computeWorthless() const;

    static void setServer(const SmartBot &bot, const Hanabi::Server &server);

private:
    static const SmartBot *bot_;
    static const Hanabi::Server *server_;

    bool cantBe_[Hanabi::NUMCOLORS][5+1];
    mutable int color_;
    mutable int value_;
    mutable trivalue playable_;
    mutable trivalue valuable_;
    mutable trivalue worthless_;
    mutable float probabilityPlayable_;
    mutable float probabilityValuable_;
    mutable float probabilityWorthless_;
};

struct Hint {
    int fitness;
    int to;
    int color;
    int value;

    Hint();
    void give(Hanabi::Server &);
};

class SmartBot : public Hanabi::Bot {

    friend class CardKnowledge;

    int me_;
    int myHandSize_;  /* purely for convenience */

    /* What does each player know about his own hand? */
    std::vector<std::vector<CardKnowledge> > handKnowledge_;
    /* What cards have been played so far? */
    int playedCount_[Hanabi::NUMCOLORS][5+1];
    /* What cards in players' hands are definitely identified?
     * This table is recomputed every turn. */
    int locatedCount_[Hanabi::NUMCOLORS][5+1];
    /* What cards in players' hands are visible to me in particular?
     * This table is recomputed every turn. */
    int eyesightCount_[Hanabi::NUMCOLORS][5+1];

    bool isPlayable(const Hanabi::Server &server, Hanabi::Card card) const;
    bool isValuable(const Hanabi::Server &server, Hanabi::Card card) const;
    bool isWorthless(const Hanabi::Server &server, Hanabi::Card card) const;
    bool couldBePlayableWithValue(const Hanabi::Server &server, const CardKnowledge &knol, int value) const;
    bool couldBeValuableWithValue(const Hanabi::Server &server, const CardKnowledge &knol, int value) const;

    void updateEyesightCount(const Hanabi::Server &server);
    bool updateLocatedCount();
    void invalidateKnol(int player_index, int card_index);
    void seePublicCard(const Hanabi::Card &played_card);

    /* Returns -1 if the named player is planning to play a card on his
     * turn, or if all his cards are known to be valuable. Otherwise,
     * returns the index of his oldest not-known-valuable card. */
    int nextDiscardIndex(int player) const;

    void noValuableWarningWasGiven(const Hanabi::Server &server, int from);

    Hint bestHintForPlayer(const Hanabi::Server &server, int to) const;

    bool maybeDiscardFinesse(Hanabi::Server &server);
    bool maybePlayLowestPlayableCard(Hanabi::Server &server);
    bool maybeGiveHelpfulHint(Hanabi::Server &server);
    bool maybeGiveValuableWarning(Hanabi::Server &server);
    bool maybePlayMysteryCard(Hanabi::Server &server);
    bool maybeDiscardWorthlessCard(Hanabi::Server &server);
    bool maybeDiscardOldCard(Hanabi::Server &server);

  public:
    SmartBot(int index, int numPlayers, int handSize);
    virtual void pleaseObserveBeforeMove(const Hanabi::Server &);
    virtual void pleaseMakeMove(Hanabi::Server &);
      virtual void pleaseObserveBeforeDiscard(const Hanabi::Server &, int from, int card_index);
      virtual void pleaseObserveBeforePlay(const Hanabi::Server &, int from, int card_index);
      virtual void pleaseObserveColorHint(const Hanabi::Server &, int from, int to, Hanabi::Color color, const std::vector<int> &card_indices);
      virtual void pleaseObserveValueHint(const Hanabi::Server &, int from, int to, Hanabi::Value value, const std::vector<int> &card_indices);
    virtual void pleaseObserveAfterMove(const Hanabi::Server &);
};
