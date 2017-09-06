
#ifndef BOTNAME
#error "BOTNAME must be defined! Pass -DBOTNAME=BlindBot for a trivial example."
#endif

#define stringify2(x) #x
#define stringify(x) stringify2(x)

#include <iostream>
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include "Hanabi.h"
#include stringify(BOTNAME.h)  /* This isn't really Standard-conforming. */
#include "BotFactory.h"

struct Statistics {
    int games;
    int totalScore;
    int scoreDistribution[26];
    int mulligansUsed[4];
};

double run(int numberOfPlayers)
{
    const int iterations = 2000;
    Hanabi::Server server;
    BotFactory<BOTNAME> botFactory;
    Statistics local_stats = {};

    server.srand(1);

    for (int i=0; i < iterations; ++i) {
        int score;
        try {
            score = server.runGame(botFactory, numberOfPlayers);
        } catch (const std::runtime_error& e) {
            std::cout << "Caught fatal exception \"" << e.what() << "\" from Hanabi server" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        assert(score == server.currentScore());
        assert(0 <= server.mulligansUsed() && server.mulligansUsed() <= 3);
        local_stats.totalScore += score;
        local_stats.scoreDistribution[score] += 1;
        local_stats.mulligansUsed[server.mulligansUsed()] += 1;
    }

    return local_stats.totalScore / (double)iterations;
}

int TesterA = 42;

int main()
{
    for (TesterA = 0; TesterA <= 1; ++TesterA) {
        double two = run(2);
        double three = run(3);
        double four = run(4);
        double five = run(5);
        printf("%d: %8.3f %8.3f %8.3f %8.3f\n", TesterA, two, three, four, five);
    }
}
