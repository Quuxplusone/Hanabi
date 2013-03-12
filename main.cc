
#ifndef BOTNAME
#error "BOTNAME must be defined! Pass -DBOTNAME=BlindBot for a trivial example."
#endif

#define stringify2(x) #x
#define stringify(x) stringify2(x)

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include "Hanabi.h"
#include stringify(BOTNAME.h)  /* This isn't really Standard-conforming. */
#include "BotFactory.h"

int main()
{
    std::srand(std::time(NULL));

    Hanabi::Server server;
    BotFactory<BOTNAME> botFactory;

    server.setLog(&std::cerr);
    int score = server.runGame(botFactory, 3);

    assert(score == server.currentScore());

    std::cout << stringify(BOTNAME) " scored " << score << " points in that first game.\n";

    server.setLog(NULL);

    const int NUMGAMES = 100000;
    int sum = score;
    int perfectGames = (score == 25);
    for (int i=2; i <= NUMGAMES; ++i) {
        int score = server.runGame(botFactory, 3);
        assert(score == server.currentScore());
        sum += score;
        perfectGames += (score == 25);
        if ((i % 2000) == 0) {
            std::cout << "Over " << i << " games, " stringify(BOTNAME) " scored an average of "
                      << (sum / (double)i) << " points per game.\n";
            if (perfectGames != 0) {
                std::cout << "  " << 100*(perfectGames / (double)i) << " percent were perfect games.\n";
            }
        }
    }

    return 0;
}
