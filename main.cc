
#ifndef BOTNAME
#error "BOTNAME must be defined! Pass -DBOTNAME=BlindBot for a trivial example."
#endif

#define stringify2(x) #x
#define stringify(x) stringify2(x)

#include <iostream>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include "Hanabi.h"
#include stringify(BOTNAME.h)  /* This isn't really Standard-conforming. */
#include "BotFactory.h"

#ifdef _OPENMP
#include <omp.h>
/* Apple's llvm-gcc 4.2.1 cannot handle __builtin_expect() inside an
 * OpenMP section. __builtin_expect() is used by assert(). This is the
 * simplest workaround. */
#define __builtin_expect(exp,c) (exp)
#endif /* _OPENMP */

struct Statistics {
    int games;
    int totalScore;
    int scoreDistribution[26];
    int mulligansUsed[4];
};

static void run_iterations(int seed, int numberOfPlayers, int iterations, Statistics &global_stats)
{
    Hanabi::Server server;
    BotFactory<BOTNAME> botFactory;
    Statistics local_stats = {};

    server.srand(seed);

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

    #pragma omp critical (update_and_dump_stats)
    {
        global_stats.games += iterations;
        global_stats.totalScore += local_stats.totalScore;
        for (int s=0; s <= 25; ++s) {
            global_stats.scoreDistribution[s] += local_stats.scoreDistribution[s];
        }
        for (int i=0; i < 4; ++i) {
            global_stats.mulligansUsed[i] += local_stats.mulligansUsed[i];
        }
    }
}

static void dump_stats(Statistics stats, bool produceHistogram)
{
    #pragma omp critical (update_and_dump_stats)
    {
        const double dgames = stats.games;
        const int perfectGames = stats.scoreDistribution[25];

        std::cout << "Over " << stats.games << " games, " stringify(BOTNAME) " scored an average of "
                  << (stats.totalScore / dgames) << " points per game.\n";
        if (perfectGames != 0) {
            const double winRate = 100*(perfectGames / dgames);
            std::cout << "  " << winRate << " percent were perfect games.\n";
        }
        if (stats.mulligansUsed[0] != stats.games) {
            std::cout << "  Mulligans used: 0 (" << 100*(stats.mulligansUsed[0] / dgames)
                      << "%); 1 (" << 100*(stats.mulligansUsed[1] / dgames)
                      << "%); 2 (" << 100*(stats.mulligansUsed[2] / dgames)
                      << "%); 3 (" << 100*(stats.mulligansUsed[3] / dgames) << "%).\n";
        }
        if (produceHistogram) {
            std::cout << "Histogram:\n";
            for (int s = 0; s <= 25; ++s) {
                if (stats.scoreDistribution[s] == 0) continue;
                const double pct = 100*(stats.scoreDistribution[s] / dgames);
                std::cout << "  " << (s < 10 ? " " : "") << s << ": " << stats.scoreDistribution[s]
                          << " (" << pct << "%)\n";
            }
        }
    }
}

static void usage(const char *why)
{
    std::cout << why << "\n"
              << "Usage:\n"
              << "    ./run_" stringify(BOTNAME) " [options]\n"
              << "        --quiet\n"
              << "        --seed <0..>\n"
              << "        --deck < input.deck (piped onto stdin)\n"
              << "        --players <2..10>\n"
              << "        --games <1..2000000000>\n"
              << "        --every <1..2000000000>\n";
    std::exit(EXIT_FAILURE);
}

static void run_one_stacked_deck_game(int numberOfPlayers)
{
    /* Get the deck to use, from stdin. */
    static const char colornames[] = "roygb";
    int counts[Hanabi::NUMCOLORS][6] = {};
    std::vector<Hanabi::Card> stackedDeck;
    std::string cardRepresentation;
    for (size_t i=0; i < (Hanabi::NUMCOLORS * 10); ++i) {
        if (std::cin >> cardRepresentation) {
            if (cardRepresentation.length() != 2) usage("Bad card in stacked deck.");
            if (cardRepresentation[0] < '1' || '5' < cardRepresentation[0]) usage("Bad card in stacked deck.");
            const char* colorname = strchr(colornames, cardRepresentation[1]);
            if (colorname == NULL) usage("Bad card in stacked deck.");
            Hanabi::Color color = (Hanabi::Color)(colorname - colornames);
            int value = (cardRepresentation[0] - '0');
            counts[color][value] += 1;
            if (counts[color][value] > Hanabi::Card(color,value).count()) usage("Too many instances of some particular card in stacked deck.");
            stackedDeck.push_back(Hanabi::Card(color,value));
        } else {
            usage("Not enough cards in stacked deck. (Should be 50.)");
        }
    }
    /* If we get here, then by the pigeonhole principle every count must be correct. */
    /* Run one game with logging to stderr. */
    Hanabi::Server server;
    BotFactory<BOTNAME> botFactory;
    server.setLog(&std::cerr);
    int score = server.runGame(botFactory, numberOfPlayers, stackedDeck);
    std::cout << stringify(BOTNAME) " scored " << score << " points in that game.\n";
    std::cout << "Mulligans used: " << server.mulligansUsed() << std::endl;
}

int main(int argc, char **argv)
{
    int numberOfPlayers = 3;
    int numberOfGames = 1000*1000;
    int every = 20000;
    int seed = -1;
    bool quiet = false;
    bool stackTheDeck = false;
    bool produceHistogram = false;

    for (int i=1; i < argc; ++i) {
        if (argv[i] == std::string("--quiet")) {
            quiet = true;
        } else if (argv[i] == std::string("--deck")) {
            stackTheDeck = true;
        } else if (argv[i] == std::string("--histogram")) {
            produceHistogram = true;
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
            ++i;
        } else if (argv[i] == std::string("--every")) {
            if (i+1 >= argc) usage("Option --every requires an argument.");
            every = atoi(argv[i+1]);
            if (every <= 0) usage("Invalid number for option --every.");
            ++i;
        } else if (argv[i] == std::string("--seed")) {
            if (i+1 >= argc) usage("Option --seed requires an argument.");
            seed = atoi(argv[i+1]);
            if (seed < 0) usage("Invalid number for option --seed.");
            ++i;
        } else {
            usage("Invalid option.");
        }
    }

    if (seed <= 0) {
        std::srand(std::time(NULL));
        seed = std::rand();
    }
    printf("--seed %d\n", seed);

    if (stackTheDeck) {
        run_one_stacked_deck_game(numberOfPlayers);
        return 0;
    }

    if (!quiet) {
        /* Run one game with logging to stderr. */
        Hanabi::Server server;
        BotFactory<BOTNAME> botFactory;
        server.setLog(&std::cerr);
        server.srand(seed);
        int score = server.runGame(botFactory, numberOfPlayers);
        std::cout << stringify(BOTNAME) " scored " << score << " points in that first game.\n";
    }

    /* Run a million games, in parallel if possible. */

#ifdef _OPENMP
    #pragma omp parallel
    if (omp_get_thread_num() == 0)
    {
        std::cout << "Running with " << omp_get_num_threads() << " threads...\n";
    }
#endif /* _OPENMP */

    Statistics stats = {};
    const int loops = numberOfGames / every;
    #pragma omp parallel for
    for (int i=0; i < loops; ++i) {
        run_iterations(seed + i, numberOfPlayers, every, stats);
        assert(stats.games % every == 0);
        dump_stats(stats, produceHistogram);
    }

    if ((numberOfGames % every) != 0) {
        run_iterations(seed + loops, numberOfPlayers, (numberOfGames % every), stats);
        dump_stats(stats, produceHistogram);
    }

    return 0;
}
