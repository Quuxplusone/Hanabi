
#include "Hanabi.h"

class CheatBot final : public Hanabi::Bot {

    int me_;

    bool maybeEnablePlay(Hanabi::Server &, int plus);
    bool maybePlayLowestPlayableCard(Hanabi::Server &);
    bool maybeDiscardWorthlessCard(Hanabi::Server &);
    bool maybeDiscardDuplicateCard(Hanabi::Server &);
    bool maybePlayProbabilities(Hanabi::Server &);
    bool maybeTemporize(Hanabi::Server &);
    void discardHighestCard(Hanabi::Server &);
    bool tryHardToDisposeOf(Hanabi::Server &, int card_index);

  public:
    CheatBot(int index, int numPlayers, int handSize);
    void pleaseObserveBeforeMove(const Hanabi::Server &) override;
    void pleaseMakeMove(Hanabi::Server &) override;
      void pleaseObserveBeforeDiscard(const Hanabi::Server &, int from, int card_index) override;
      void pleaseObserveBeforePlay(const Hanabi::Server &, int from, int card_index) override;
      void pleaseObserveColorHint(const Hanabi::Server &, int from, int to, Hanabi::Color color, Hanabi::CardIndices card_indices) override;
      void pleaseObserveValueHint(const Hanabi::Server &, int from, int to, Hanabi::Value value, Hanabi::CardIndices card_indices) override;
    void pleaseObserveAfterMove(const Hanabi::Server &) override;
};
