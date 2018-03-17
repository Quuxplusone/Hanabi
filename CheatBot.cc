
#include <cassert>
#include "Hanabi.h"
#include "CheatBot.h"

using namespace Hanabi;

static int numPlayers;
static std::vector<std::vector<Card> > hands;
static std::vector<Card> discards;

/* Apple's llvm-gcc 4.2.1 does not support this #pragma,
 * and GCC in general does not support it with non-POD
 * types such as std::vector (Bugzilla bug 27557).
 * The appropriate workaround in both cases is to build
 * run_CheatBot without OpenMP enabled. */
#pragma omp threadprivate(numPlayers,hands,discards)

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
        for (int i=0; i < hands[p].size(); ++i) {
            result += (hands[p][i] == card);
        }
    }
    return result;
}

CheatBot::CheatBot(int index, int n, int /*handSize*/)
{
    me_ = index;
    numPlayers = n;
    hands.resize(n);
    discards.clear();
}

void CheatBot::pleaseObserveBeforeMove(const Server &server)
{
    if (me_ == 0) {
        discards = server.discards();
        for (int p=1; p < hands.size(); ++p) {
            hands[p] = server.handOfPlayer(p);
        }
    } else if (me_ == 1) {
        hands[0] = server.handOfPlayer(0);
    }
}

void CheatBot::pleaseObserveBeforeDiscard(const Server &, int, int) { }
void CheatBot::pleaseObserveBeforePlay(const Server &, int, int) { }
void CheatBot::pleaseObserveColorHint(const Server &, int, int, Color, CardIndices) { }
void CheatBot::pleaseObserveValueHint(const Server &, int, int, Value, CardIndices) { }
void CheatBot::pleaseObserveAfterMove(const Server &) { }

bool CheatBot::maybeEnablePlay(Server &server, int plus)
{
    const int partner = (me_ + plus) % numPlayers;
    assert(partner != me_);

    int lowest_value = 5;
    int best_index = -1;

    for (int i=0; i < hands[me_].size(); ++i) {
        Card card = hands[me_][i];
        if (card.value >= lowest_value) continue;
        if (!server.pileOf(card.color).nextValueIs(card.value)) continue;

        Card nextCard(card.color, card.value+1);
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

bool CheatBot::maybePlayLowestPlayableCard(Server &server)
{
    for (int plus = 1; plus < numPlayers; ++plus) {
        if (maybeEnablePlay(server, plus)) return true;
    }

    int lowest_value = 10;
    int best_index = -1;
    for (int i=0; i < hands[me_].size(); ++i) {
        Card card = hands[me_][i];
        if (server.pileOf(card.color).nextValueIs(card.value)) {
            if (card.value < lowest_value) {
                best_index = i;
                lowest_value = card.value;
            }
        }
    }

    if (best_index != -1) {
        server.pleasePlay(best_index);
        return true;
    }

    return false;
}

static bool noPlayableCardsVisible(const Server &server)
{
    for (int p=0; p < hands.size(); ++p) {
        for (int i=0; i < hands[p].size(); ++i) {
            Card card = hands[p][i];
            if (server.pileOf(card.color).nextValueIs(card.value)) {
                return false;
            }
        }
    }
    return true;
}

static bool noWorthlessOrDuplicateCardsVisible(const Server& server)
{
    for (int p=0; p < hands.size(); ++p) {
        for (int i=0; i < hands[p].size(); ++i) {
            Card card = hands[p][i];
            Pile pile = server.pileOf(card.color);
            if (pile.contains(card.value)) return false;  /* it's worthless */
            assert(card.value > pile.size());
            for (int v = pile.size()+1; v < card.value; ++v) {
                Card earlier_card(card.color, v);
                if (vector_count(discards, earlier_card) == earlier_card.count()) {
                    /* earlier_card is gone for good, so this later card
                     * is also unplayable (worthless). */
                    return false;
                }
            }
            if (visibleCopiesOf(card) >= 2) return false;  /* it's a duplicate */
        }
    }
    return true;
}

bool CheatBot::tryHardToDisposeOf(Server& server, int card_index)
{
    if (server.discardingIsAllowed()) {
        server.pleaseDiscard(card_index);
        return true;
    } else if (server.mulligansRemaining() >= 2) {
        /* We REALLY want to get this card out of our hand! */
        server.pleasePlay(card_index);
        return true;
    } else {
        /* There's no way to dispose of this card. */
        return false;
    }
}

bool CheatBot::maybeDiscardWorthlessCard(Server &server)
{
    for (int i=0; i < hands[me_].size(); ++i) {
        Card card = hands[me_][i];
        Pile pile = server.pileOf(card.color);
        if (pile.contains(card.value)) {
            /* This card won't ever be needed again. */
            return tryHardToDisposeOf(server, i);
        } else if (vector_count(hands[me_], card) >= 2) {
            /* I've got two copies of this card already; it's definitely safe
             * to discard one of them. */
            return tryHardToDisposeOf(server, i);
        } else {
            assert(card.value > pile.size());
            for (int v = pile.size()+1; v < card.value; ++v) {
                Card earlier_card(card.color, v);
                if (vector_count(discards, earlier_card) == earlier_card.count()) {
                    /* earlier_card is gone for good, so this later card
                     * is also unplayable. */
                    return tryHardToDisposeOf(server, i);
                }
            }
        }
    }
    return false;
}

bool CheatBot::maybeDiscardDuplicateCard(Server &server)
{
    if (!server.discardingIsAllowed()) return false;

    for (int i=0; i < hands[me_].size(); ++i) {
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
    if (!server.discardingIsAllowed()) return false;

    int bestGap = 0;
    int bestIndex = -1;

    for (int i=0; i < hands[me_].size(); ++i) {
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
    for (int i=1; i < hands[me_].size(); ++i) {
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

    const int stillToGo = (25 - server.currentScore());
    const bool endgameNoMoreDiscarding = (stillToGo >= server.cardsRemainingInDeck()+1);

    assert(1 <= stillToGo && stillToGo <= 25);

    /* If the next player will have no move and be unable to temporize,
     * it might be best for me to rescue him by discarding.
     * Otherwise, if I have a playable card, play it.
     * Otherwise, if I have a disposable card and there's a hint-stone
     * to regain, go ahead and do that.
     * Otherwise, if there's a hint-stone available, temporize. */

    if (maybePlayLowestPlayableCard(server)) return;

    /* If there are no playable cards visible, then temporizing won't solve anything.
     * Someone must discard a card to get things moving again --- the sooner the better.
     */
    if (noPlayableCardsVisible(server)) {
        if (maybeDiscardWorthlessCard(server)) return;
        if (maybeDiscardDuplicateCard(server)) return;
        if (noWorthlessOrDuplicateCardsVisible(server)) {
            if (maybePlayProbabilities(server)) return;
        }
    }

    /* This heuristic isn't very scientifically motivated. */
    static const int tempo[NUMHINTS+1] = {
        0, 1, 2, 3,
        4, 5, 8, 25, 25
    };
    const bool shouldTemporizeEarly = (stillToGo <= tempo[server.hintStonesRemaining()]);

    if (endgameNoMoreDiscarding || shouldTemporizeEarly) {
        /* Emergency endgame situation. Temporizing is better than discarding. */
        if (maybeTemporize(server)) return;
        if (maybeDiscardWorthlessCard(server)) return;
        if (maybeDiscardDuplicateCard(server)) return;
        if (maybePlayProbabilities(server)) return;
    } else {
        if (maybeDiscardWorthlessCard(server)) return;
        if (maybeDiscardDuplicateCard(server)) return;
        if (maybeTemporize(server)) return;
        if (maybePlayProbabilities(server)) return;
    }

    /* Well, phooey. */
    assert(server.hintStonesRemaining() == 0);  /* because maybeTemporize() failed */
    discardHighestCard(server);
}
