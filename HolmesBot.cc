
#include <algorithm>
#include <cassert>
#include "Hanabi.h"
#include "HolmesBot.h"

using namespace Hanabi;

static const bool UseMulligans = true;

template<typename T>
static bool vector_contains(const std::vector<T> &vec, T value)
{
    for (int i=0; i < vec.size(); ++i) {
        if (vec[i] == value) return true;
    }
    return false;
}

CardKnowledge::CardKnowledge()
{
    for (Color color = RED; color <= BLUE; ++color) {
        colors_[color] = MAYBE;
    }
    for (int value = 1; value <= 5; ++value) {
        values_[value] = MAYBE;
    }
    isPlayable = false;
    isValuable = false;
    isWorthless = false;
}

bool CardKnowledge::mustBe(Hanabi::Color color) const { return (colors_[color] == YES); }
bool CardKnowledge::mustBe(Hanabi::Value value) const { return (values_[value] == YES); }
bool CardKnowledge::cannotBe(Hanabi::Color color) const { return (colors_[color] == NO); }
bool CardKnowledge::cannotBe(Hanabi::Value value) const { return (values_[value] == NO); }

int CardKnowledge::color() const
{
    for (Color k = RED; k <= BLUE; ++k) {
        if (this->mustBe(k)) return (int)k;
    }
    return -1;
}

int CardKnowledge::value() const
{
    for (int v = 1; v <= 5; ++v) {
        if (this->mustBe(Value(v))) return v;
    }
    return -1;
}

void CardKnowledge::setMustBe(Hanabi::Color color)
{
    assert(colors_[color] != NO);
    for (Color k = RED; k <= BLUE; ++k) {
        colors_[k] = ((k == color) ? YES : NO);
    }
}

void CardKnowledge::setMustBe(Hanabi::Value value)
{
    assert(values_[value] != NO);
    for (int v = 1; v <= 5; ++v) {
        values_[v] = ((v == value) ? YES : NO);
    }
}

void CardKnowledge::update(const Server &server, const HolmesBot &bot)
{
    while (true) {
        bool restart = false;
        int color = this->color();
        int value = this->value();

        if (color == -1) {
            for (Color k = RED; k <= BLUE; ++k) {
                if (colors_[k] == NO) continue;
                else if (color == -1) color = k;
                else { color = -1; break; }
            }
            if (color != -1) this->setMustBe(Color(color));
        }

        if (value == -1) {
            for (int v = 1; v <= 5; ++v) {
                if (values_[v] == NO) continue;
                else if (value == -1) value = v;
                else { value = -1; break; }
            }
            if (value != -1) this->setMustBe(Value(value));
        }

        /* If we do know the card's color, see if we can identify its value
         * based on what we know about its properties. */
        if (value == -1 && color != -1) {
            for (int v = 1; v <= 5; ++v) {
                if (values_[v] == NO) continue;
                const int total = Card(Color(color),v).count();
                const int played = bot.playedCount_[color][v];
                const int held = bot.locatedCount_[color][v];
                assert(played+held <= total);
                if ((played+held == total) ||
                    (isValuable && played != total-1) ||
                    (isWorthless && played != -1))
                {
                    values_[v] = NO;
                    restart = true;
                }
            }
            if (restart) continue;
        }

        /* If we know the card's value, see if we can identify its color
         * based on what we know about its properties. */
        if (color == -1 && value != -1) {
            for (Color k = RED; k <= BLUE; ++k) {
                if (colors_[k] == NO) continue;
                const int total = Card(k,value).count();
                const int played = bot.playedCount_[k][value];
                const int held = bot.locatedCount_[k][value];
                assert(played+held <= total);
                if ((played+held == total) ||
                    (isValuable && played != total-1) ||
                    (isWorthless && played != -1))
                {
                    colors_[k] = NO;
                    restart = true;
                }
            }
            if (restart) continue;
        }

        /* Done. */
        break;
    }

    if (!this->isWorthless) {
        for (Color k = RED; k <= BLUE; ++k) {
            if (colors_[k] == NO) continue;
            for (int v = 5; v >= 1; --v) {
                if (values_[v] == NO) continue;
                if (!server.pileOf(k).contains(v)) {
                    goto mightBeUseful;
                }
            }
        }
        this->isWorthless = true;
        return;
      mightBeUseful:;
    }

    if (!this->isPlayable) {
        for (Color k = RED; k <= BLUE; ++k) {
            if (colors_[k] == NO) continue;
            for (int v = 5; v >= 1; --v) {
                if (values_[v] == NO) continue;
                if (!server.pileOf(k).nextValueIs(v)) {
                    goto mightBeUnplayable;
                }
            }
        }
        this->isPlayable = true;
        return;
      mightBeUnplayable:;
    }
}

Hint::Hint()
{
    information_content = -1;
    color = -1;
    value = -1;
    to = -1;
}

void Hint::give(Server &server)
{
    assert(to != -1);
    if (color != -1) {
        server.pleaseGiveColorHint(to, Color(color));
    } else if (value != -1) {
        server.pleaseGiveValueHint(to, Value(value));
    } else {
        assert(false);
    }
}

HolmesBot::HolmesBot(int index, int numPlayers)
{
    me_ = index;
    handKnowledge_.resize(numPlayers);
    for (int i=0; i < numPlayers; ++i) {
        handKnowledge_[i].resize(4);
    }
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            playedCount_[k][v] = 0;
        }
    }
}

int HolmesBot::lowestPlayableValue() const
{
    for (int v = 1; v <= 5; ++v) {
        for (Color k = RED; k <= BLUE; ++k) {
            if (playedCount_[k][v] != -1) return Value(v);
        }
    }

    /* In this case, even 5s are not playable; the game must be over! */
    return 6;
}

bool HolmesBot::couldBeValuable(int value) const
{
    if (value < 1 || 5 < value) return false;
    for (Color k = RED; k <= BLUE; ++k) {
        if (playedCount_[k][value] == Card(k,value).count()-1)
            return true;
    }
    return false;
}

void HolmesBot::invalidateKnol(int player_index, int card_index)
{
    /* The other cards are shifted down and a new one drawn at the end. */
    std::vector<CardKnowledge> &vec = handKnowledge_[player_index];
    assert(vec.size() == 4);
    for (int i = card_index; i+1 < vec.size(); ++i) {
        vec[i] = vec[i+1];
    }
    vec.back() = CardKnowledge();
}

void HolmesBot::seePublicCard(const Card &card)
{
    int &entry = this->playedCount_[card.color][card.value];
    if (entry == -1) return;
    entry += 1;
    assert(1 <= entry && entry <= card.count());
}

bool HolmesBot::updateLocatedCount()
{
    int newCount[Hanabi::NUMCOLORS][5+1] = {};

    for (int p=0; p < handKnowledge_.size(); ++p) {
        for (int i=0; i < 4; ++i) {
            CardKnowledge &knol = handKnowledge_[p][i];
            int k = knol.color();
            int v = knol.value();
            if (k != -1 && v != -1) {
                newCount[k][v] += 1;
            }
        }
    }

    if (memcmp(this->locatedCount_, newCount, sizeof newCount) != 0) {
        memcpy(this->locatedCount_, newCount, sizeof newCount);
        return true;  /* there was a change */
    }
    return false;
}

void HolmesBot::pleaseObserveBeforeMove(const Server &server)
{
    assert(server.whoAmI() == me_);

    memset(this->locatedCount_, '\0', sizeof this->locatedCount_);
    this->updateLocatedCount();
    do {
        for (int p=0; p < handKnowledge_.size(); ++p) {
            for (int i=0; i < 4; ++i) {
                CardKnowledge &knol = handKnowledge_[p][i];
                knol.update(server, *this);
            }
        }
    } while (this->updateLocatedCount());
}

void HolmesBot::pleaseObserveBeforeDiscard(const Hanabi::Server &server, int from, int card_index)
{
    assert(server.whoAmI() == me_);
    this->invalidateKnol(from, card_index);
    this->seePublicCard(server.activeCard());
}

void HolmesBot::pleaseObserveBeforePlay(const Hanabi::Server &server, int from, int card_index)
{
    assert(server.whoAmI() == me_);

    Card card = server.activeCard();

    assert(!handKnowledge_[from][card_index].isWorthless);
    if (handKnowledge_[from][card_index].isValuable) {
        /* We weren't wrong about this card being valuable, were we? */
        assert(this->playedCount_[card.color][card.value] == card.count()-1);
    }

    this->invalidateKnol(from, card_index);

    if (server.pileOf(card.color).nextValueIs(card.value)) {
        /* This card is getting played, not discarded. */
        if (this->playedCount_[card.color][card.value] != card.count()-1) {
            this->wipeOutPlayables(card);
        }
        this->playedCount_[card.color][card.value] = -1;  /* we no longer care about it */
    } else {
        this->seePublicCard(card);
    }
}

void HolmesBot::pleaseObserveColorHint(const Hanabi::Server &server, int from, int to, Color color, const std::vector<int> &card_indices)
{
    assert(server.whoAmI() == me_);

    /* Someone has given me a color hint. Using HolmesBot's strategy,
     * this means that all the named cards are playable; except for
     * any whose values I already know, which I can deduce for myself
     * whether they're playable or not. */

    Pile pile = server.pileOf(color);
    int value = pile.size() + 1;

    assert(1 <= value && value <= 5);

    for (int i=0; i < card_indices.size(); ++i) {
        CardKnowledge &knol = handKnowledge_[to][card_indices[i]];
        knol.setMustBe(color);
        if (knol.value() == -1) {
            knol.setMustBe(Value(value));
        }
    }
}

int HolmesBot::nextDiscardIndex(int to) const
{
    for (int i=0; i < 4; ++i) {
        if (handKnowledge_[to][i].isPlayable) return -1;
        if (handKnowledge_[to][i].isWorthless) return -1;
    }
    for (int i=0; i < 4; ++i) {
        if (!handKnowledge_[to][i].isValuable) return i;
    }
    return -1;
}

void HolmesBot::pleaseObserveValueHint(const Hanabi::Server &server, int from, int to, Value value, const std::vector<int> &card_indices)
{
    assert(server.whoAmI() == me_);

    /* Someone has given Bob a value hint. If the named cards
     * include the one Bob would normally be discarding next,
     * then this must be a warning that that card is valuable.
     * Otherwise, all the named cards are playable. */

    const int discardIndex = this->nextDiscardIndex(to);
    const int lowestValue = lowestPlayableValue();
    const bool isPointless = (value < lowestValue);
    const bool isWarning = couldBeValuable(value) && vector_contains(card_indices, discardIndex);

    assert(!isPointless);

    if (isWarning) {
        assert(discardIndex != -1);
        handKnowledge_[to][discardIndex].isValuable = true;
        if (value == lowestValue) {
            /* This card is valuable, i.e., not worthless; therefore it
             * must be playable sometime in the future. And since it's
             * the lowest playable value already, it must in fact be
             * playable right now! But we can't say the same thing for
             * any of the other named cards. */
            handKnowledge_[to][discardIndex].isPlayable = true;
        }
    }

    for (int i=0; i < card_indices.size(); ++i) {
        CardKnowledge &knol = handKnowledge_[to][card_indices[i]];
        knol.setMustBe(value);
        if (!isWarning && !isPointless) {
            knol.isPlayable = true;
        }
    }
}

void HolmesBot::pleaseObserveAfterMove(const Hanabi::Server &server)
{
    assert(server.whoAmI() == me_);
}

void HolmesBot::wipeOutPlayables(const Card &played_card)
{
    const int numPlayers = handKnowledge_.size();
    for (int player = 0; player < numPlayers; ++player) {
        for (int c = 0; c < 4; ++c) {
            CardKnowledge &knol = handKnowledge_[player][c];
            if (!knol.isPlayable) continue;
            if (knol.mustBe(Value(5))) continue;
            if (knol.cannotBe(played_card.color)) continue;
            if (knol.cannotBe(played_card.value)) continue;
            /* This card might or might not be playable, anymore. */
            knol.isPlayable = false;
        }
    }
}

bool HolmesBot::maybePlayLowestPlayableCard(Server &server)
{
    /* Find the lowest-valued playable card in my hand.
     * Notice that this has the useful side-effect of preferring to play
     * cards whose values are unknown (-1) but which have been deduced
     * to be playable by CardKnowledge::update(). */
    int best_index = -1;
    int best_value = 6;
    for (int i=0; i < 4; ++i) {
        const CardKnowledge &knol = handKnowledge_[me_][i];
        if (knol.isPlayable && knol.value() < best_value) {
            best_index = i;
            best_value = knol.value();
        }
    }

    /* If I found a card to play, play it. */
    if (best_index != -1) {
        assert(1 <= best_value && best_value <= 5);
        server.pleasePlay(best_index);
        return true;
    }

    return false;
}

bool HolmesBot::maybeDiscardWorthlessCard(Server &server)
{
    for (int i=0; i < 4; ++i) {
        const CardKnowledge &knol = handKnowledge_[me_][i];
        if (knol.isWorthless) {
            server.pleaseDiscard(i);
            return true;
        }
    }

    return false;
}

Hint HolmesBot::bestHintForPlayer(const Server &server, int partner) const
{
    assert(partner != me_);
    const std::vector<Card> partners_hand = server.handOfPlayer(partner);

    bool is_really_playable[4];
    for (int c=0; c < 4; ++c) {
        is_really_playable[c] =
            server.pileOf(partners_hand[c].color).nextValueIs(partners_hand[c].value);
    }

    Hint best_so_far;
    best_so_far.to = partner;

    /* Can we construct a color hint that gives our partner information
     * about unknown-playable cards, without also including any
     * unplayable cards? */
    for (Color color = RED; color <= BLUE; ++color) {
        int information_content = 0;
        bool misinformative = false;
        for (int c=0; c < 4; ++c) {
            const CardKnowledge &knol = handKnowledge_[partner][c];
            if (partners_hand[c].color != color) continue;
            if (is_really_playable[c] && !knol.isPlayable) {
                information_content += 1;
            } else if (!is_really_playable[c] && (knol.value() == -1)) {
                misinformative = true;
                break;
            }
        }
        if (misinformative) continue;
        if (information_content > best_so_far.information_content) {
            best_so_far.information_content = information_content;
            best_so_far.color = color;
            best_so_far.value = -1;
        }
    }

    /* Avoid giving hints that could be misinterpreted as warnings. */
    const int discardIndex = nextDiscardIndex(partner);
    int valueToAvoid = -1;
    if (discardIndex != -1) {
        valueToAvoid = partners_hand[discardIndex].value;
        if (!couldBeValuable(valueToAvoid)) valueToAvoid = -1;
    }

    for (int value = 1; value <= 5; ++value) {
        if (value == valueToAvoid) continue;
        int information_content = 0;
        bool misinformative = false;
        for (int c=0; c < 4; ++c) {
            if (partners_hand[c].value != value) continue;
            if (is_really_playable[c] &&
                !handKnowledge_[partner][c].isPlayable)
            {
                information_content += 1;
            } else if (!is_really_playable[c]) {
                misinformative = true;
                break;
            }
        }
        if (misinformative) continue;
        if (information_content > best_so_far.information_content) {
            best_so_far.information_content = information_content;
            best_so_far.color = -1;
            best_so_far.value = value;
        }
    }

    return best_so_far;
}

bool HolmesBot::maybeGiveValuableWarning(Server &server)
{
    const int numPlayers = handKnowledge_.size();
    const int player_to_warn = (me_ + 1) % numPlayers;

    /* Is the player to our left just about to discard a card
     * that is really valuable? */
    int discardIndex = this->nextDiscardIndex(player_to_warn);
    if (discardIndex == -1) return false;
    Card targetCard = server.handOfPlayer(player_to_warn)[discardIndex];
    if (playedCount_[targetCard.color][targetCard.value] != targetCard.count()-1) {
        /* The target card isn't actually valuable. Good. */
        return false;
    }

    /* Oh no! Warn him before he discards it! */
    assert(playedCount_[targetCard.color][targetCard.value] != -1);
    assert(!handKnowledge_[player_to_warn][discardIndex].isValuable);
    assert(!handKnowledge_[player_to_warn][discardIndex].isPlayable);
    assert(!handKnowledge_[player_to_warn][discardIndex].isWorthless);

    /* Sometimes we just can't give a hint. */
    if (server.hintStonesRemaining() == 0) return false;

    Hint bestHint = bestHintForPlayer(server, player_to_warn);
    if (bestHint.information_content > 0) {
        /* Excellent; we found a hint that will cause him to play a card
         * instead of discarding. */
        bestHint.give(server);
        return true;
    }

    /* Otherwise, we'll have to give a warning. */
    if (targetCard.value == lowestPlayableValue()) {
        assert(server.pileOf(targetCard.color).nextValueIs(targetCard.value));
    } else {
        assert(targetCard.value > lowestPlayableValue());
    }

    server.pleaseGiveValueHint(player_to_warn, targetCard.value);
    return true;
}

bool HolmesBot::maybeGiveHelpfulHint(Server &server)
{
    if (server.hintStonesRemaining() == 0) return false;

    const int numPlayers = handKnowledge_.size();
    Hint bestHint;
    for (int i = 1; i < numPlayers; ++i) {
        const int partner = (me_ + i) % numPlayers;
        Hint candidate = bestHintForPlayer(server, partner);
        if (candidate.information_content > bestHint.information_content) {
            bestHint = candidate;
        }
    }

    if (bestHint.information_content <= 0) return false;

    /* Give the hint. */
    bestHint.give(server);
    return true;
}

bool HolmesBot::maybePlayMysteryCard(Server &server)
{
    if (!UseMulligans) return false;

    const int table[4] = { -99, 1, 2, 4 };
    if (server.cardsRemainingInDeck() <= table[server.mulligansRemaining()]) {
        /* We could temporize, or we could do something that forces us to
         * draw a card. If we got here, temporizing has been rejected as
         * an option; so let's do something that forces us to draw a card.
         * At this point, we might as well try to play something random
         * and hope we get lucky. */
        for (int i=3; i >= 0; --i) {
            const CardKnowledge &knol = handKnowledge_[me_][i];
            assert(!knol.isPlayable);  /* or we would have played it already */
            if (knol.isWorthless) continue;
            if (knol.color() != -1 && knol.value() != -1) {
                /* A known card shouldn't be playable. */
                assert(!server.pileOf(Color(knol.color())).nextValueIs(knol.value()));
                continue;
            }
            server.pleasePlay(i);
            return true;
        }
    }
    return false;
}

bool HolmesBot::maybeDiscardOldCard(Server &server)
{
    for (int i=0; i < 4; ++i) {
        const CardKnowledge &knol = handKnowledge_[me_][i];
        assert(!knol.isPlayable);
        if (knol.isValuable) continue;
        server.pleaseDiscard(i);
        return true;
    }
    /* I didn't find anything I was willing to discard. */
    return false;
}

void HolmesBot::pleaseMakeMove(Server &server)
{
    assert(server.whoAmI() == me_);
    assert(server.activePlayer() == me_);
    assert(UseMulligans || !server.mulligansUsed());

    /* If I have a playable card, play it.
     * Otherwise, if someone else has an unknown-playable card, hint it.
     * Otherwise, just discard my oldest (index-0) card. */

    if (maybeGiveValuableWarning(server)) return;
    if (maybePlayLowestPlayableCard(server)) return;
    if (maybeGiveHelpfulHint(server)) return;

    /* We couldn't find a good hint to give, or else we're out of hint-stones.
     * Discard a card. */

    if (maybePlayMysteryCard(server)) return;
    if (maybeDiscardWorthlessCard(server)) return;
    if (maybeDiscardOldCard(server)) return;

    /* In this unfortunate case, which still happens fairly often, I find
     * that my whole hand is composed of valuable cards, and I just have
     * to discard the one of them that will block our progress the least. */
    int best_index = 0;
    for (int i=0; i < 4; ++i) {
        assert(handKnowledge_[me_][i].isValuable);
        if (handKnowledge_[me_][i].value() > handKnowledge_[me_][best_index].value()) {
            best_index = i;
        }
    }
    server.pleaseDiscard(best_index);
}
