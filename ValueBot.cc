
#include <algorithm>
#include <cassert>
#include "Hanabi.h"
#include "ValueBot.h"

using namespace Hanabi;

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

ValueBot::ValueBot(int index, int numPlayers, int handSize)
{
    me_ = index;
    handKnowledge_.resize(numPlayers);
    for (int i=0; i < numPlayers; ++i) {
        handKnowledge_[i].resize(handSize);
    }
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            cardCount_[k][v] = 0;
        }
    }
}

int ValueBot::lowestPlayableValue() const
{
    for (int v = 1; v <= 5; ++v) {
        for (Color k = RED; k <= BLUE; ++k) {
            if (cardCount_[k][v] != -1) return v;
        }
    }

    /* In this case, even 5s are not playable; the game must be over! */
    return 6;
}

bool ValueBot::couldBeValuable(int value) const
{
    if (value < 1 || 5 < value) return false;
    for (Color k = RED; k <= BLUE; ++k) {
        if (cardCount_[k][value] == Card(k,value).count()-1)
            return true;
    }
    return false;
}

void ValueBot::invalidateKnol(int player_index, int card_index)
{
    /* The other cards are shifted down and a new one drawn at the end. */
    std::vector<CardKnowledge> &vec = handKnowledge_[player_index];
    for (int i = card_index; i+1 < vec.size(); ++i) {
        vec[i] = vec[i+1];
    }
    vec.back() = CardKnowledge();
}

void ValueBot::seePublicCard(const Card &card)
{
    int &entry = this->cardCount_[card.color][card.value];
    if (entry == -1) return;
    entry += 1;
    assert(1 <= entry && entry <= card.count());
}

void ValueBot::makeThisValueWorthless(const Hanabi::Server &server, Value value)
{
    const int numPlayers = handKnowledge_.size();
    for (int player = 0; player < numPlayers; ++player) {
        for (int index = 0; index < server.sizeOfHandOfPlayer(player); ++index) {
            CardKnowledge &knol = handKnowledge_[player][index];
            if (knol.mustBe(value)) {
                assert(!knol.isValuable);
                knol.isWorthless = true;
            }
        }
    }
}

void ValueBot::pleaseObserveBeforeMove(const Server &server)
{
    assert(server.whoAmI() == me_);
    myHandSize_ = server.sizeOfHandOfPlayer(me_);
}

void ValueBot::pleaseObserveBeforeDiscard(const Hanabi::Server &server, int from, int card_index)
{
    assert(server.whoAmI() == me_);
    this->invalidateKnol(from, card_index);
    this->seePublicCard(server.activeCard());
}

void ValueBot::pleaseObserveBeforePlay(const Hanabi::Server &server, int from, int card_index)
{
    assert(server.whoAmI() == me_);

    Card card = server.activeCard();

    assert(!handKnowledge_[from][card_index].isWorthless);
    if (handKnowledge_[from][card_index].isValuable) {
        /* We weren't wrong about this card being valuable, were we? */
        assert(this->cardCount_[card.color][card.value] == card.count()-1);
    }

    this->invalidateKnol(from, card_index);

    if (server.pileOf(card.color).nextValueIs(card.value)) {
        /* This card is getting played, not discarded. */
        if (this->cardCount_[card.color][card.value] != card.count()-1) {
            this->wipeOutPlayables(server, card);
        }
        this->cardCount_[card.color][card.value] = -1;  /* we no longer care about it */
        if (lowestPlayableValue() > card.value) {
            /* A whole bunch of cards just dropped below the "lowest playable value"
             * rank. Mark them as unplayable. */
            this->makeThisValueWorthless(server, card.value);
        }
    } else {
        this->seePublicCard(card);
    }
}

void ValueBot::pleaseObserveColorHint(const Hanabi::Server &server, int /*from*/, int to, Color color, Hanabi::CardIndices card_indices)
{
    assert(server.whoAmI() == me_);

    /* Someone has given me a color hint. Using ValueBot's strategy,
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
        const int count = cardCount_[color][knol.value()];
        if (count == -1) {
            knol.isWorthless = true;
        } else {
            if (value == knol.value()) {
                knol.isPlayable = true;
            }
            if (count == Card(color,knol.value()).count()-1) {
                knol.isValuable = true;
            }
        }
    }
}

int ValueBot::nextDiscardIndex(const Hanabi::Server& server, int to) const
{
    int numCards = server.sizeOfHandOfPlayer(to);
    for (int i=0; i < numCards; ++i) {
        if (handKnowledge_[to][i].isPlayable) return -1;
        if (handKnowledge_[to][i].isWorthless) return -1;
    }
    for (int i=0; i < numCards; ++i) {
        if (!handKnowledge_[to][i].isValuable) return i;
    }
    return -1;
}

void ValueBot::pleaseObserveValueHint(const Hanabi::Server &server, int from, int to, Value value, Hanabi::CardIndices card_indices)
{
    assert(server.whoAmI() == me_);

    /* Someone has given Bob a value hint. If the named cards
     * include the one Bob would normally be discarding next,
     * then this must be a warning that that card is valuable.
     * Otherwise, all the named cards are playable. */

    const int discardIndex = this->nextDiscardIndex(server, to);
    const int lowestValue = lowestPlayableValue();
    const bool isPointless = (value < lowestValue);
    const bool isWarning = couldBeValuable(value) && card_indices.contains(discardIndex);
    const bool isHintStoneReclaim =
        (!server.discardingIsAllowed()) &&
        (from == (to+1) % server.numPlayers()) &&
        card_indices.contains(0);

    if (isHintStoneReclaim) {
        return;
    }

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

void ValueBot::pleaseObserveAfterMove(const Hanabi::Server &server)
{
    assert(server.whoAmI() == me_);
}

void ValueBot::wipeOutPlayables(const Hanabi::Server &server, const Card &played_card)
{
    const int numPlayers = handKnowledge_.size();
    for (int player = 0; player < numPlayers; ++player) {
        for (int c = 0; c < server.sizeOfHandOfPlayer(player); ++c) {
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

bool ValueBot::maybePlayLowestPlayableCard(Server &server)
{
    /* Find the lowest-valued playable card in my hand. */
    int best_index = -1;
    int best_value = 6;
    for (int i=0; i < myHandSize_; ++i) {
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

bool ValueBot::maybeDiscardWorthlessCard(Server &server)
{
    for (int i=0; i < myHandSize_; ++i) {
        const CardKnowledge &knol = handKnowledge_[me_][i];
        if (knol.isWorthless) {
            server.pleaseDiscard(i);
            return true;
        }
    }

    return false;
}

Hint ValueBot::bestHintForPlayer(const Server &server, int partner) const
{
    assert(partner != me_);
    const std::vector<Card> partners_hand = server.handOfPlayer(partner);

    bool is_really_playable[5];
    for (int c=0; c < partners_hand.size(); ++c) {
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
        for (int c=0; c < partners_hand.size(); ++c) {
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
    const int discardIndex = nextDiscardIndex(server, partner);
    int valueToAvoid = -1;
    if (discardIndex != -1) {
        valueToAvoid = partners_hand[discardIndex].value;
        if (!couldBeValuable(valueToAvoid)) valueToAvoid = -1;
    }

    for (int value = 1; value <= 5; ++value) {
        if (value == valueToAvoid) continue;
        int information_content = 0;
        bool misinformative = false;
        for (int c=0; c < partners_hand.size(); ++c) {
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

bool ValueBot::maybeGiveValuableWarning(Server &server)
{
    if (server.hintStonesRemaining() == 0) return false;

    const int numPlayers = handKnowledge_.size();
    const int player_to_warn = (me_ + 1) % numPlayers;

    /* Is the player to our left just about to discard a card
     * that is really valuable? */
    int discardIndex = this->nextDiscardIndex(server, player_to_warn);
    if (discardIndex == -1) return false;
    Card targetCard = server.handOfPlayer(player_to_warn)[discardIndex];
    if (cardCount_[targetCard.color][targetCard.value] != targetCard.count()-1) {
        /* The target card isn't actually valuable. Good. */
        return false;
    }

    /* Oh no! Warn him before he discards it! */
    assert(cardCount_[targetCard.color][targetCard.value] != -1);
    assert(!handKnowledge_[player_to_warn][discardIndex].isValuable);
    assert(!handKnowledge_[player_to_warn][discardIndex].isPlayable);
    assert(!handKnowledge_[player_to_warn][discardIndex].isWorthless);
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

bool ValueBot::maybeGiveHelpfulHint(Server &server)
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

bool ValueBot::maybeDiscardOldCard(Server &server)
{
    for (int i=0; i < myHandSize_; ++i) {
        const CardKnowledge &knol = handKnowledge_[me_][i];
        assert(!knol.isPlayable);
        if (knol.isValuable) continue;
        server.pleaseDiscard(i);
        return true;
    }
    /* I didn't find anything I was willing to discard. */
    return false;
}

void ValueBot::pleaseMakeMove(Server &server)
{
    assert(server.whoAmI() == me_);
    assert(server.activePlayer() == me_);

    /* If I have a playable card, play it.
     * Otherwise, if someone else has an unknown-playable card, hint it.
     * Otherwise, just discard my oldest (index-0) card. */

    if (maybeGiveValuableWarning(server)) return;
    if (maybePlayLowestPlayableCard(server)) return;
    if (maybeGiveHelpfulHint(server)) return;

    /* We couldn't find a good hint to give, or else we're out of hint-stones.
     * Discard a card. However, discarding is not allowed when we have all
     * the hint stones, so in that case, just hint to the player on our right
     * about his oldest card. */

    if (!server.discardingIsAllowed()) {
        const int numPlayers = server.numPlayers();
        const int right_partner = (me_ + numPlayers - 1) % numPlayers;
        server.pleaseGiveValueHint(right_partner, server.handOfPlayer(right_partner)[0].value);
    } else {
        if (maybeDiscardWorthlessCard(server)) return;
        if (maybeDiscardOldCard(server)) return;

        /* In this unfortunate case, which still happens fairly often, I find
         * that my whole hand is composed of valuable cards, and I just have
         * to discard the one of them that will block our progress the least. */
        int best_index = 0;
        for (int i=0; i < myHandSize_; ++i) {
            assert(handKnowledge_[me_][i].isValuable);
            if (handKnowledge_[me_][i].value() > handKnowledge_[me_][best_index].value()) {
                best_index = i;
            }
        }
        server.pleaseDiscard(best_index);
    }
}
