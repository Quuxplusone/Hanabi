
#include <cassert>
#include "Hanabi.h"
#include "CheatBot.h"

using namespace Hanabi;

static int numPlayers;
static std::vector<std::vector<Card> > hands;
static std::vector<Card> discards;
static bool endgameNoMoreDiscarding;

template<typename T>
static int vector_count(const std::vector<T> &vec, T value)
{
    int result = 0;
    for (int i=0; i < vec.size(); ++i) {
        result += (vec[i] == value);
    }
    return result;
}

static int visibleCopiesOf(Card card)
{
    int result = 0;
    for (int p=0; p < hands.size(); ++p) {
        for (int i=0; i < 4; ++i) {
            result += (hands[p][i] == card);
        }
    }
    return result;
}

static int cardsLeftToPlay(const Server &server)
{
    int result = 0;
    for (Color color = RED; color <= BLUE; ++color) {
        result += (5 - server.pileOf(color).size());
    }
    return result;
}

CheatBot::CheatBot(int index, int n)
{
    me_ = index;
    numPlayers = n;
    hands.resize(n);
    discards.clear();
}

void CheatBot::pleaseObserveBeforeMove(const Server &server)
{
    if (me_ == 0) {
        for (int p=1; p < hands.size(); ++p) {
            hands[p] = server.handOfPlayer(p);
        }
    } else if (me_ == 1) {
        hands[0] = server.handOfPlayer(0);
    }
}

void CheatBot::pleaseObserveBeforeDiscard(const Server &server, int from, int card_index)
{
    if (me_ == 0) {
        discards.push_back(server.activeCard());
    }
}

void CheatBot::pleaseObserveBeforePlay(const Server &, int, int) { }
void CheatBot::pleaseObserveColorHint(const Server &, int, int, Color, const std::vector<int> &) { }
void CheatBot::pleaseObserveValueHint(const Server &, int, int, Value, const std::vector<int> &) { }
void CheatBot::pleaseObserveAfterMove(const Server &) { }

bool CheatBot::maybeEnablePlay(Server &server, int plus)
{
    const int partner = (me_ + plus) % numPlayers;
    if (partner == me_) return false;

    int lowest_value = 5;
    int best_index = -1;

    for (int i=0; i < 4; ++i) {
        Card card = hands[me_][i];
        if (card.value >= lowest_value) continue;
        if (!server.pileOf(card.color).nextValueIs(card.value)) continue;

        Card nextCard = card;
        nextCard.value = Value(card.value+1);
        assert(nextCard.count() != 0);
        if (vector_count(hands[partner], nextCard) != 0) {
            lowest_value = card.value;
            best_index = i;
        }
    }
    if (best_index != -1) {
        assert(1 <= lowest_value && lowest_value <= 4);
        server.pleasePlay(best_index);
        return true;
    }
    return false;
}

bool CheatBot::maybeRescueNextPlayer(Server &server, int plus)
{
    const int partner = (me_ + plus) % numPlayers;
    if (partner == me_) return false;

    for (int i=0; i < 4; ++i) {
        Card card = hands[partner][i];
        if (server.pileOf(card.color).nextValueIs(card.value)) return false;
    }

    /* The next player has nothing to play! Can I rescue him by a play
     * of my own? For example, I could play the red 3 to make his red 4
     * playable. */

    if (maybeEnablePlay(server, plus)) return true;

    /* I can't enable any play for him, which means the best I could do
     * is discard in order to allow him to temporize. However, if we're
     * in the endgame, play/discard is no worse than discard/temporize. */
    if (endgameNoMoreDiscarding) return false;

    /* Eh, he can just temporize. */
    if (server.hintStonesRemaining() > 0) return false;

    for (int i=0; i < 4; ++i) {
        Card card = hands[partner][i];
        if (server.pileOf(card.color).contains(card.value)) {
            /* This card is worthless. He can just discard it. */
            return false;
        }
    }

    /* Let's return a hint stone so that he can temporize. */
    return maybeDiscardWorthlessCard(server) || maybePlayProbabilities(server);
}

bool CheatBot::maybePlayLowestPlayableCard(Server &server)
{
    for (int plus = 1; plus < numPlayers; ++plus) {
        if (maybeEnablePlay(server, plus)) return true;
    }

    int lowest_value = 10;
    int best_index = -1;
    for (int i=0; i < 4; ++i) {
        Card card = hands[me_][i];
        if (server.pileOf(card.color).nextValueIs(card.value)) {
            if (card.value < lowest_value) {
                best_index = i;
                lowest_value = card.value;
            }
        }
    }

    if (best_index != -1) {
        Card card = hands[me_][best_index];
        server.pleasePlay(best_index);
        return true;
    }

    return false;
}

bool CheatBot::maybeDiscardWorthlessCard(Server &server)
{
    for (int i=0; i < 4; ++i) {
        Card card = hands[me_][i];
        if (server.pileOf(card.color).contains(card.value)) {
            /* This card won't ever be needed again. */
            server.pleaseDiscard(i);
            return true;
        }
    }

    for (int i=0; i < 4; ++i) {
        Card card = hands[me_][i];
        if (visibleCopiesOf(card) > 1) {
            /* We have a duplicate of this card somewhere visible. */
            server.pleaseDiscard(i);
            return true;
        }
    }
    return false;
}

bool CheatBot::maybePlayProbabilities(Server &server)
{
    int bestGap = 0;
    int bestIndex = -1;

    for (int i=0; i < 4; ++i) {
        Card card = hands[me_][i];
        if (card.value == 5) continue;
        /* This codepath should be reached only after discarding
         * any worthless card; so none of my cards should be
         * worthless at this point. */
        Pile pile = server.pileOf(card.color);
        assert(!pile.contains(card.value));

        int gap = (card.value - pile.size());
        assert(gap >= 1);
        if (gap > bestGap) {
            if (vector_count(discards, card) == card.count()-1) {
                /* This is really the last copy: don't discard it! */
            } else {
                /* There's another copy coming up later. */
                bestGap = gap;
                bestIndex = i;
            }
        }
    }

    if (bestIndex != -1) {
        server.pleaseDiscard(bestIndex);
        return true;
    }

    return false;
}

bool CheatBot::maybeTemporize(Server &server)
{
    if (server.hintStonesRemaining() == 0) return false;

    const int nextPlayer = (me_ + 1) % hands.size();
    server.pleaseGiveValueHint(nextPlayer, hands[nextPlayer][0].value);
    return true;
}

void CheatBot::discardHighestCard(Server &server)
{
    int bestIndex = 0;
    for (int i=1; i < 4; ++i) {
        Card card = hands[me_][i];
        if (card.value > hands[me_][bestIndex].value) {
            bestIndex = i;
        }
    }
    server.pleaseDiscard(bestIndex);
}

void CheatBot::pleaseMakeMove(Server &server)
{
    assert(server.whoAmI() == me_);
    assert(server.activePlayer() == me_);

    endgameNoMoreDiscarding = (cardsLeftToPlay(server) >= server.cardsRemainingInDeck()+1);

    /* If the next player will have no move and be unable to temporize,
     * it might be best for me to rescue him by discarding.
     * Otherwise, if I have a playable card, play it.
     * Otherwise, if I have a disposable card and there's a hint-stone
     * to regain, go ahead and do that.
     * Otherwise, if there's a hint-stone available, temporize. */

    if (maybeRescueNextPlayer(server, 1)) return;
    if (maybeRescueNextPlayer(server, 2)) return;
    if (maybePlayLowestPlayableCard(server)) return;

    const int stillToGo = cardsLeftToPlay(server);
    assert(1 <= stillToGo && stillToGo <= 25);

    /* This heuristic isn't very scientifically motivated. */
    static const int tempo[] = {
        0, 1, 2, 3,
        4, 4, 8, 25
    };
    const bool shouldTemporizeEarly = (stillToGo <= tempo[server.hintStonesRemaining()]);

    if (endgameNoMoreDiscarding || shouldTemporizeEarly) {
        /* Emergency endgame situation. Temporizing is better than discarding. */
        if (maybeTemporize(server)) return;
        if (maybeDiscardWorthlessCard(server)) return;
        if (maybePlayProbabilities(server)) return;
    } else {
        if (maybeDiscardWorthlessCard(server)) return;
        if (maybeTemporize(server)) return;
        if (maybePlayProbabilities(server)) return;
    }

    /* Well, phooey. */
    discardHighestCard(server);
}
