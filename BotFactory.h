
#ifndef H_BOT_FACTORY
#define H_BOT_FACTORY

#include "Hanabi.h"

template<class SpecificBot>
struct BotFactory final : public Hanabi::BotFactory
{
    Hanabi::Bot *create(int index, int numPlayers, int handSize) const override { return new SpecificBot(index, numPlayers, handSize); }
    void destroy(Hanabi::Bot *bot) const override { delete bot; }
};

#endif /* H_BOT_FACTORY */
