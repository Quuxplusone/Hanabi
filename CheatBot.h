
#include "Hanabi.h"

class CheatBot : public Hanabi::Bot {

    int me_;

    bool maybeRescueNextPlayer(Hanabi::Server &, int plus);
    bool maybeEnablePlay(Hanabi::Server &, int plus);
    bool maybePlayLowestPlayableCard(Hanabi::Server &);
    bool maybeDiscardWorthlessCard(Hanabi::Server &);
    bool maybePlayProbabilities(Hanabi::Server &);
    bool maybeTemporize(Hanabi::Server &);
    void discardHighestCard(Hanabi::Server &);

  public:
    CheatBot(int index, int numPlayers);
    virtual void pleaseObserveBeforeMove(const Hanabi::Server &);
    virtual void pleaseMakeMove(Hanabi::Server &);
      virtual void pleaseObserveBeforeDiscard(const Hanabi::Server &, int from, int card_index);
      virtual void pleaseObserveBeforePlay(const Hanabi::Server &, int from, int card_index);
      virtual void pleaseObserveColorHint(const Hanabi::Server &, int from, int to, Hanabi::Color color, const std::vector<int> &card_indices);
      virtual void pleaseObserveValueHint(const Hanabi::Server &, int from, int to, Hanabi::Value value, const std::vector<int> &card_indices);
    virtual void pleaseObserveAfterMove(const Hanabi::Server &);
};
