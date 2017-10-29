
#include "Hanabi.h"

class NewCheatBot final : public Hanabi::Bot {
    int me_;

  public:
    NewCheatBot(int index, int numPlayers, int handSize);
    void pleaseObserveBeforeMove(const Hanabi::Server &) override;
    void pleaseMakeMove(Hanabi::Server &) override;
      void pleaseObserveBeforeDiscard(const Hanabi::Server &, int from, int card_index) override;
      void pleaseObserveBeforePlay(const Hanabi::Server &, int from, int card_index) override;
      void pleaseObserveColorHint(const Hanabi::Server &, int from, int to, Hanabi::Color color, const std::vector<int> &card_indices) override;
      void pleaseObserveValueHint(const Hanabi::Server &, int from, int to, Hanabi::Value value, const std::vector<int> &card_indices) override;
    void pleaseObserveAfterMove(const Hanabi::Server &) override;
};
