
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

    int sum = 0;
    int perfectGames = 0;
    int mulligansUsed[4] = {};

    for (int i=1; i <= 1000000; ++i) {
        int score = server.runGame(botFactory, 3);
        assert(score == server.currentScore());
        assert(0 <= server.mulligansUsed() && server.mulligansUsed() <= 3);
        sum += score;
        perfectGames += (score == 25);
        mulligansUsed[server.mulligansUsed()] += 1;
        if (i == 1) {
            std::cout << stringify(BOTNAME) " scored " << score << " points in that first game.\n";
            server.setLog(NULL);
        } else if ((i % 20000) == 0) {
            std::cout << "Over " << i << " games, " stringify(BOTNAME) " scored an average of "
                      << (sum / (double)i) << " points per game.\n";
            if (perfectGames != 0) {
                std::cout << "  " << 100*(perfectGames / (double)i) << " percent were perfect games.\n";
            }
            if (mulligansUsed[0] != i) {
                std::cout << "  Mulligans used: 0 (" << 100*(mulligansUsed[0] / (double)i)
                          << "%); 1 (" << 100*(mulligansUsed[1] / (double)i)
                          << "%); 2 (" << 100*(mulligansUsed[2] / (double)i)
                          << "%); 3 (" << 100*(mulligansUsed[3] / (double)i) << "%).\n";
            }
        }
    }

    return 0;
}
