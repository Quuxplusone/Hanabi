
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

struct Statistics {
    int games;
    int totalScore;
    int perfectGames;
    int mulligansUsed[4];
};

static void run_iterations(int numberOfPlayers, int iterations, Statistics &global_stats)
{
    Hanabi::Server server;
    BotFactory<BOTNAME> botFactory;
    Statistics local_stats = {};

    for (int i=0; i < iterations; ++i) {
        int score = server.runGame(botFactory, numberOfPlayers);
        assert(score == server.currentScore());
        assert(0 <= server.mulligansUsed() && server.mulligansUsed() <= 3);
        local_stats.totalScore += score;
        local_stats.perfectGames += (score == 25);
        local_stats.mulligansUsed[server.mulligansUsed()] += 1;
    }

    /* Ideally this stat-summation would be an atomic transaction. */
    global_stats.games += iterations;
    global_stats.totalScore += local_stats.totalScore;
    global_stats.perfectGames += local_stats.perfectGames;
    for (int i=0; i < 4; ++i) {
        global_stats.mulligansUsed[i] += local_stats.mulligansUsed[i];
    }
}

static void dump_stats(Statistics stats)
{
    double dgames = stats.games;

    std::cout << "Over " << stats.games << " games, " stringify(BOTNAME) " scored an average of "
              << (stats.totalScore / dgames) << " points per game.\n";
    if (stats.perfectGames != 0) {
        std::cout << "  " << 100*(stats.perfectGames / dgames) << " percent were perfect games.\n";
    }
    if (stats.mulligansUsed[0] != stats.games) {
        std::cout << "  Mulligans used: 0 (" << 100*(stats.mulligansUsed[0] / dgames)
                  << "%); 1 (" << 100*(stats.mulligansUsed[1] / dgames)
                  << "%); 2 (" << 100*(stats.mulligansUsed[2] / dgames)
                  << "%); 3 (" << 100*(stats.mulligansUsed[3] / dgames) << "%).\n";
    }
}

static void usage(const char *why)
{
    std::cout << why << "\n"
              << "Usage:\n"
              << "    ./run_" stringify(BOTNAME) "Bot [options]\n"
              << "        --quiet\n"
              << "        --players <2..10>\n"
              << "        --games <1000..2000000000>\n"
              << "        --every <1000..2000000000>\n";
    std::exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int numberOfPlayers = 3;
    int numberOfGames = 1000*1000;
    int every = 20000;
    bool logOneGame = true;

    for (int i=1; i < argc; ++i) {
        if (argv[i] == std::string("--quiet")) {
            logOneGame = false;
        } else if (argv[i] == std::string("--players")) {
            if (i+1 >= argc) usage("Option --players requires an argument.");
            numberOfPlayers = atoi(argv[i+1]);
            if (numberOfPlayers < 2) usage("Invalid number of players.");
            if (numberOfPlayers > 10) usage("Invalid number of players.");
            ++i;
        } else if (argv[i] == std::string("--games")) {
            if (i+1 >= argc) usage("Option --games requires an argument.");
            numberOfGames = atoi(argv[i+1]);
            if (numberOfGames <= 0) usage("Invalid number of games.");
            if ((numberOfGames % 1000) != 0) usage("Number of games must be divisible by 1000.");
            ++i;
        } else if (argv[i] == std::string("--every")) {
            if (i+1 >= argc) usage("Option --every requires an argument.");
            every = atoi(argv[i+1]);
            if (every <= 0) usage("Invalid number for option --every.");
            if ((every % 1000) != 0) usage("Number for option --every must be divisible by 1000.");
            ++i;
        } else {
            usage("Invalid option.");
        }
    }

    std::srand(std::time(NULL));

    if (logOneGame) {
        /* Run one game with logging to stderr. */
        Hanabi::Server server;
        BotFactory<BOTNAME> botFactory;
        server.setLog(&std::cerr);
        int score = server.runGame(botFactory, numberOfPlayers);
        std::cout << stringify(BOTNAME) " scored " << score << " points in that first game.\n";
    }

    /* Run a million games, in parallel if possible. */
    Statistics stats = {};
    for (int i=0; i < numberOfGames/1000; ++i) {
        run_iterations(numberOfPlayers, 1000, stats);
        const int games = stats.games;
        if ((stats.games % every) == 0) {
            dump_stats(stats);
        }
    }

    if ((numberOfGames % every) != 0) {
        dump_stats(stats);
    }

    return 0;
}
