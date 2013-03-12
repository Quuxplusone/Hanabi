
#include <cassert>
#include <cstdlib>
#include "Hanabi.h"
#include "BlindBot.h"

using namespace Hanabi;

BlindBot::BlindBot(int index, int numPlayers) { }
void BlindBot::pleaseObserveBeforeMove(const Server &) { }
void BlindBot::pleaseObserveBeforeDiscard(const Server &, int from, int card_index) { }
void BlindBot::pleaseObserveBeforePlay(const Server &, int from, int card_index) { }
void BlindBot::pleaseObserveColorHint(const Server &, int from, int to, Color color, std::vector<int> card_indices) { }
void BlindBot::pleaseObserveValueHint(const Server &, int from, int to, Value value, std::vector<int> card_indices) { }
void BlindBot::pleaseObserveAfterMove(const Server &) { }

void BlindBot::pleaseMakeMove(Server &server)
{
    /* Just try to play a random card from my hand. */
    int random_index = std::rand() % 4;
    server.pleasePlay(random_index);
}
