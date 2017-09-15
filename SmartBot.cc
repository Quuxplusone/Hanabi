
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include "Hanabi.h"
#include "SmartBot.h"

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

template<typename T>
static int vector_count(const std::vector<T> &vec, T value)
{
    int result = 0;
    for (int i=0; i < vec.size(); ++i) {
        if (vec[i] == value) result += 1;
    }
    return result;
}

const SmartBot *CardKnowledge::bot_;
const Hanabi::Server *CardKnowledge::server_;
void CardKnowledge::setServer(const SmartBot &bot, const Hanabi::Server &server)
{
    bot_ = &bot;
    server_ = &server;
}

CardKnowledge::CardKnowledge()
{
    color_ = -2;
    value_ = -2;
    std::memset(cantBe_, '\0', sizeof cantBe_);
    playable_ = valuable_ = worthless_ = MAYBE;
    probabilityPlayable_ = probabilityValuable_ = probabilityWorthless_ = -1.0;
}

bool CardKnowledge::mustBe(Hanabi::Color color) const { computeIdentity(); return (this->color_ == color); }
bool CardKnowledge::mustBe(Hanabi::Value value) const { computeIdentity(); return (this->value_ == value); }
bool CardKnowledge::cannotBe(Hanabi::Card card) const { return cantBe_[card.color][card.value]; }
bool CardKnowledge::cannotBe(Hanabi::Color color) const
{
    if (this->color_ >= 0) return (this->color_ != color);
    for (int v = 1; v <= 5; ++v) {
        if (!cantBe_[color][v]) return false;
    }
    return true;
}

bool CardKnowledge::cannotBe(Hanabi::Value value) const
{
    if (this->value_ >= 0) return (this->value_ != value);
    for (Color k = RED; k <= BLUE; ++k) {
        if (!cantBe_[k][value]) return false;
    }
    return true;
}

void CardKnowledge::befuddleByDiscard()
{
    /* A discard could make things valuable that were not valuable before;
     * or make things worthless (if a valuable card was discarded).
     */
    valuable_ = MAYBE; probabilityValuable_ = -1.0f;
    if (worthless_ != YES) { worthless_ = MAYBE; probabilityWorthless_ = -1.0f; }
}

void CardKnowledge::befuddleByPlay(bool success)
{
    /* A successful play could make new things playable; or make duplicates of
     * the played card worthless. A failed play is equivalent to a discard.
     */
    if (success) {
        playable_ = MAYBE; probabilityPlayable_ = -1.0f;
    } else {
        if (valuable_ != NO) { valuable_ = MAYBE; probabilityValuable_ = -1.0f; }
    }
    if (worthless_ != YES) { worthless_ = MAYBE; probabilityWorthless_ = -1.0f; }
}

void CardKnowledge::setMustBe(Hanabi::Color color)
{
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            if (k != color) cantBe_[k][v] = true;
        }
    }
    color_ = color;
    if (value_ == -1) value_ = -2;
    if (playable_ == MAYBE) probabilityPlayable_ = -1.0;
    if (valuable_ == MAYBE) probabilityValuable_ = -1.0;
    if (worthless_ == MAYBE) probabilityWorthless_ = -1.0;
}

void CardKnowledge::setMustBe(Hanabi::Value value)
{
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            if (v != value) cantBe_[k][v] = true;
        }
    }
    if (color_ == -1) color_ = -2;
    value_ = value;
    if (playable_ == MAYBE) probabilityPlayable_ = -1.0;
    if (valuable_ == MAYBE) probabilityValuable_ = -1.0;
    if (worthless_ == MAYBE) probabilityWorthless_ = -1.0;
}

void CardKnowledge::setCannotBe(Hanabi::Color color)
{
    for (int v = 1; v <= 5; ++v) {
        cantBe_[color][v] = true;
    }
    if (color_ == -1) color_ = -2;
    if (value_ == -1) value_ = -2;
    if (playable_ == MAYBE) probabilityPlayable_ = -1.0;
    if (valuable_ == MAYBE) probabilityValuable_ = -1.0;
    if (worthless_ == MAYBE) probabilityWorthless_ = -1.0;
}

void CardKnowledge::setCannotBe(Hanabi::Value value)
{
    for (Color k = RED; k <= BLUE; ++k) {
        cantBe_[k][value] = true;
    }
    if (color_ == -1) color_ = -2;
    if (value_ == -1) value_ = -2;
    if (playable_ == MAYBE) probabilityPlayable_ = -1.0;
    if (valuable_ == MAYBE) probabilityValuable_ = -1.0;
    if (worthless_ == MAYBE) probabilityWorthless_ = -1.0;
}

void CardKnowledge::setIsPlayable(const Server& server, bool knownPlayable)
{
    for (Color k = RED; k <= BLUE; ++k) {
        int playableValue = server.pileOf(k).size() + 1;
        for (int v = 1; v <= 5; ++v) {
            if (this->cantBe_[k][v]) continue;
            if ((v == playableValue) != knownPlayable) {
                this->cantBe_[k][v] = true;
            }
        }
    }
    if (color_ == -1) color_ = -2;
    if (value_ == -1) value_ = -2;
    playable_ = (knownPlayable ? YES : NO);
    probabilityPlayable_ = (knownPlayable ? 1.0 : 0.0);
    if (valuable_ == MAYBE) probabilityValuable_ = -1.0;
    if (worthless_ == MAYBE) probabilityWorthless_ = -1.0;
}

void CardKnowledge::setIsValuable(const SmartBot &bot, const Server& server, bool knownValuable)
{
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            if (this->cantBe_[k][v]) continue;
            if (bot.isValuable(server, Card(k,v)) != knownValuable) {
                this->cantBe_[k][v] = true;
            }
        }
    }
    if (color_ == -1) color_ = -2;
    if (value_ == -1) value_ = -2;
    if (playable_ == MAYBE) probabilityPlayable_ = -1.0;
    valuable_ = (knownValuable ? YES : NO);
    probabilityValuable_ = (knownValuable ? 1.0 : 0.0);
    if (worthless_ == MAYBE) probabilityWorthless_ = -1.0;
}

void CardKnowledge::setIsWorthless(const SmartBot &bot, const Server& server, bool knownWorthless)
{
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            if (this->cantBe_[k][v]) continue;
            if (bot.isWorthless(server, Card(k,v)) != knownWorthless) {
                this->cantBe_[k][v] = true;
            }
        }
    }
    if (color_ == -1) color_ = -2;
    if (value_ == -1) value_ = -2;
    if (playable_ == MAYBE) probabilityPlayable_ = -1.0;
    if (valuable_ == MAYBE) probabilityValuable_ = -1.0;
    worthless_ = (knownWorthless ? YES : NO);
    probabilityWorthless_ = (knownWorthless ? 1.0 : 0.0);
}

void CardKnowledge::computeIdentity() const
{
    if (color_ != -2 && value_ != -2) return;
    int color = -2;
    int value = -2;
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            if (this->cantBe_[k][v]) continue;
            color = (color == -2 || color == k) ? k : -1;
            value = (value == -2 || value == v) ? v : -1;
        }
    }
    assert(color != -2);
    assert(value != -2);
    color_ = color;
    value_ = value;
}

void CardKnowledge::computePlayable() const
{
    if (probabilityPlayable_ != -1.0f) return;
    int total_count = 0;
    int yes_count = 0;
    for (Color k = RED; k <= BLUE; ++k) {
        int playableValue = server_->pileOf(k).size() + 1;
        for (int v = 1; v <= 5; ++v) {
            if (this->cantBe_[k][v]) continue;
            total_count += 1;
            yes_count += (v == playableValue);
        }
    }
    assert(total_count >= 1);
    probabilityPlayable_ = (float)yes_count / total_count;
    playable_ = (yes_count == total_count) ? YES : (yes_count != 0) ? MAYBE : NO;
}

void CardKnowledge::computeValuable() const
{
    if (probabilityValuable_ != -1.0f) return;
    int total_count = 0;
    int yes_count = 0;
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            if (this->cantBe_[k][v]) continue;
            total_count += 1;
            yes_count += bot_->isValuable(*server_, Card(k, v));
        }
    }
    assert(total_count >= 1);
    probabilityValuable_ = (float)yes_count / total_count;
    valuable_ = (yes_count == total_count) ? YES : (yes_count != 0) ? MAYBE : NO;
}

void CardKnowledge::computeWorthless() const
{
    if (probabilityWorthless_ != -1.0f) return;
    int total_count = 0;
    int yes_count = 0;
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            if (this->cantBe_[k][v]) continue;
            total_count += 1;
            yes_count += bot_->isWorthless(*server_, Card(k, v));
        }
    }
    assert(total_count >= 1);
    probabilityWorthless_ = (float)yes_count / total_count;
    worthless_ = (yes_count == total_count) ? YES : (yes_count != 0) ? MAYBE : NO;
}

template<bool useMyEyesight>
void CardKnowledge::update()
{
    /* Rule out any cards that have been completely played and/or discarded. */
    if (!known()) {
        /* If this card is not known, then it cannot be any of the specific cards
         * listed in locatedCount_/eyesightCount_. Notice that if this card HAS
         * been identified, then it WILL be represented in locatedCount_, by
         * definition, and so we should skip this logic in the "known" case. */
        bool recompute = false;
        for (Color k = RED; k <= BLUE; ++k) {
            for (int v = 1; v <= 5; ++v) {
                if (this->cantBe_[k][v]) continue;
                const int total = (v == 1 ? 3 : (v == 5 ? 1 : 2));
                const int played = bot_->playedCount_[k][v];
                const int held = (useMyEyesight ? bot_->eyesightCount_[k][v] : bot_->locatedCount_[k][v]);
                assert(played+held <= total);
                if (played+held == total) {
                    this->cantBe_[k][v] = true;
                    recompute = true;
                }
            }
        }
        if (recompute) {
            color_ = -2;
            value_ = -2;
            playable_ = valuable_ = worthless_ = MAYBE;
            probabilityPlayable_ = probabilityValuable_ = probabilityWorthless_ = -1.0f;
        }
    }
}

Hint::Hint()
{
    fitness = -1;
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

SmartBot::SmartBot(int index, int numPlayers, int handSize)
{
    me_ = index;
    handKnowledge_.resize(numPlayers);
    for (int i=0; i < numPlayers; ++i) {
        handKnowledge_[i].resize(handSize);
    }
    std::memset(playedCount_, '\0', sizeof playedCount_);
}

bool SmartBot::isPlayable(const Server &server, Card card) const
{
    const int playableValue = server.pileOf(card.color).size() + 1;
    return (card.value == playableValue);
}

bool SmartBot::isValuable(const Server &server, Card card) const
{
    /* A card which has not yet been played, and which is the
     * last of its kind, is valuable. */
    if (playedCount_[card.color][card.value] != card.count()-1) return false;
    return !this->isWorthless(server, card);
}

bool SmartBot::isWorthless(const Server &server, Card card) const
{
    const int playableValue = server.pileOf(card.color).size() + 1;
    if (card.value < playableValue) return true;
    /* If all the red 4s are in the discard pile, then the red 5 is worthless. */
    while (card.value > playableValue) {
        --card.value;
        if (playedCount_[card.color][card.value] == card.count()) return true;
    }
    return false;
}

/* Could "knol" be playable, if it were known to be of value "value"? */
bool SmartBot::couldBePlayableWithValue(const Server &server, const CardKnowledge &knol, int value) const
{
    if (value < 1 || 5 < value) return false;
    if (knol.playable() != MAYBE) return false;
    for (Color k = RED; k <= BLUE; ++k) {
        Card card(k, value);
        if (knol.cannotBe(card)) continue;
        if (this->isPlayable(server, card))
            return true;
    }
    return false;
}

/* Could "knol" be valuable, if it were known to be of value "value"? */
bool SmartBot::couldBeValuableWithValue(const Server &server, const CardKnowledge &knol, int value) const
{
    if (value < 1 || 5 < value) return false;
    if (knol.valuable() != MAYBE) return false;
    for (Color k = RED; k <= BLUE; ++k) {
        Card card(k, value);
        if (knol.cannotBe(card)) continue;
        if (this->isValuable(server, card))
            return true;
    }
    return false;
}

void SmartBot::invalidateKnol(int player_index, int card_index)
{
    /* The other cards are shifted down and a new one drawn at the end. */
    std::vector<CardKnowledge> &vec = handKnowledge_[player_index];
    for (int i = card_index; i+1 < vec.size(); ++i) {
        vec[i] = vec[i+1];
    }
    vec.back() = CardKnowledge();
}

void SmartBot::seePublicCard(const Card &card)
{
    int &entry = this->playedCount_[card.color][card.value];
    entry += 1;
    assert(1 <= entry && entry <= card.count());
}

void SmartBot::updateEyesightCount(const Server &server)
{
    std::memset(this->eyesightCount_, '\0', sizeof this->eyesightCount_);

    const int numPlayers = handKnowledge_.size();
    for (int p=0; p < numPlayers; ++p) {
        if (p == me_) {
            for (int i=0; i < myHandSize_; ++i) {
                CardKnowledge &knol = handKnowledge_[p][i];
                if (knol.known()) {
                    this->eyesightCount_[knol.color()][knol.value()] += 1;
                }
            }
        } else {
            const std::vector<Card> hand = server.handOfPlayer(p);
            for (int i=0; i < hand.size(); ++i) {
                const Card &card = hand[i];
                this->eyesightCount_[card.color][card.value] += 1;
            }
        }
    }
}

bool SmartBot::updateLocatedCount()
{
    int newCount[Hanabi::NUMCOLORS][5+1] = {};

    for (int p=0; p < handKnowledge_.size(); ++p) {
        for (int i=0; i < handKnowledge_[p].size(); ++i) {
            const CardKnowledge &knol = handKnowledge_[p][i];
            if (knol.known()) {
                newCount[knol.color()][knol.value()] += 1;
            }
        }
    }

    if (std::memcmp(this->locatedCount_, newCount, sizeof newCount) != 0) {
        std::memcpy(this->locatedCount_, newCount, sizeof newCount);
        return true;  /* there was a change */
    }
    return false;
}

int SmartBot::nextDiscardIndex(int to) const
{
    const int numCards = handKnowledge_[to].size();
    double best_fitness = 0;
    int best_index = -1;
    for (int i=0; i < numCards; ++i) {
        const CardKnowledge &knol = handKnowledge_[to][i];

        if (knol.playable() == YES) return -1;  /* we should just play this card */
        if (knol.worthless() == YES) return -1;  /* we should already have discarded this card */
        if (knol.valuable() == YES) continue;  /* we should never discard this card */

        double fitness = 100 + knol.probabilityWorthless();
        if (fitness > best_fitness) {
            best_fitness = fitness;
            best_index = i;
        }
    }
    return best_index;
}

void SmartBot::noValuableWarningWasGiven(const Hanabi::Server &server, int from)
{
    /* Something just happened that wasn't a warning. If what happened
     * wasn't a hint to the guy expecting a warning, then he can safely
     * deduce that his card isn't valuable enough to warn about. */

    /* The rules are different when there are no cards left to draw,
     * or when valuable-warning hints can't be given. */
    if (server.cardsRemainingInDeck() == 0) return;
    if (server.hintStonesRemaining() == 0) return;

    const int playerExpectingWarning = (from + 1) % handKnowledge_.size();
    const int discardIndex = this->nextDiscardIndex(playerExpectingWarning);

    if (discardIndex != -1) {
        handKnowledge_[playerExpectingWarning][discardIndex].setIsValuable(*this, server, false);
    }
}

void SmartBot::pleaseObserveBeforeMove(const Server &server)
{
    CardKnowledge::setServer(*this, server);
    assert(server.whoAmI() == me_);

    myHandSize_ = server.sizeOfHandOfPlayer(me_);

    for (int p=0; p < handKnowledge_.size(); ++p) {
        const int numCards = server.sizeOfHandOfPlayer(p);
        assert(handKnowledge_[p].size() >= numCards);
        handKnowledge_[p].resize(numCards);
    }

    std::memset(this->locatedCount_, '\0', sizeof this->locatedCount_);
    this->updateLocatedCount();
    do {
        for (int p=0; p < handKnowledge_.size(); ++p) {
            const int numCards = handKnowledge_[p].size();
            for (int i=0; i < numCards; ++i) {
                CardKnowledge &knol = handKnowledge_[p][i];
                knol.update<false>();
            }
        }
    } while (this->updateLocatedCount());

    this->updateEyesightCount(server);

    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            assert(this->locatedCount_[k][v] <= this->eyesightCount_[k][v]);
        }
    }
}

void SmartBot::pleaseObserveBeforeDiscard(const Hanabi::Server &server, int from, int card_index)
{
    CardKnowledge::setServer(*this, server);
    assert(server.whoAmI() == me_);
    Card card = server.activeCard();

    this->noValuableWarningWasGiven(server, from);

    const CardKnowledge& knol = handKnowledge_[from][card_index];
    if (knol.known() && knol.playable() == YES) {
        /* Alice is discarding a playable card whose value she knows.
         * This indicates a "discard finesse": she can see someone at the table
         * with that same card as their newest card. Look around the table. If
         * you can't see who has that card, it must be you. */
        const int numPlayers = handKnowledge_.size();
        bool seenIt = false;
        for (int partner = 0; partner < numPlayers; ++partner) {
            if (partner == from || partner == me_) continue;
            Card newestCard = server.handOfPlayer(partner).back();
            if (newestCard == card) {
                handKnowledge_[partner].back().setMustBe(card.color);
                handKnowledge_[partner].back().setMustBe(card.value);
                seenIt = true;
                break;
            }
        }
        if (!seenIt) {
            handKnowledge_[me_].back().setMustBe(card.color);
            handKnowledge_[me_].back().setMustBe(card.value);
        }
    }

    for (auto&& hand : handKnowledge_) {
        for (CardKnowledge &knol : hand) {
            knol.befuddleByDiscard();
        }
    }

    this->seePublicCard(card);
    this->invalidateKnol(from, card_index);
}

void SmartBot::pleaseObserveBeforePlay(const Hanabi::Server &server, int from, int card_index)
{
    CardKnowledge::setServer(*this, server);
    assert(server.whoAmI() == me_);
    Card card = server.activeCard();
    const bool success = this->isPlayable(server, card);

    this->noValuableWarningWasGiven(server, from);

#ifndef NDEBUG
    assert(handKnowledge_[from][card_index].worthless() != YES);
    if (handKnowledge_[from][card_index].valuable() == YES) {
        /* We weren't wrong about this card being valuable, were we? */
        assert(this->isValuable(server, card));
    }
#endif

    for (auto&& hand : handKnowledge_) {
        for (CardKnowledge &knol : hand) {
            knol.befuddleByPlay(success);
        }
    }

    this->seePublicCard(card);
    this->invalidateKnol(from, card_index);
}

void SmartBot::pleaseObserveColorHint(const Hanabi::Server &server, int from, int to, Color color, const std::vector<int> &card_indices)
{
    CardKnowledge::setServer(*this, server);
    assert(server.whoAmI() == me_);

    /* Alice has given Bob a color hint. Using SmartBot's strategy,
     * this means that the newest (possible) of the named cards is
     * currently playable. */

    const int playerExpectingWarning = (from + 1) % handKnowledge_.size();
    if (to != playerExpectingWarning) {
        this->noValuableWarningWasGiven(server, from);
    }

    const int numCards = server.sizeOfHandOfPlayer(to);

    bool seenPlayable = false;
    for (int i=numCards-1; i >= 0; --i) {
        CardKnowledge &knol = handKnowledge_[to][i];
        if (vector_contains(card_indices, i)) {
            const bool wasMaybePlayable = (knol.playable() == MAYBE);
            knol.setMustBe(color);
            const bool isntUnplayable = (knol.playable() != NO);
            if (wasMaybePlayable && isntUnplayable && !seenPlayable) {
                seenPlayable = true;
                knol.setIsPlayable(server, true);
            }
        } else {
            knol.setCannotBe(color);
        }
    }
}

void SmartBot::pleaseObserveValueHint(const Hanabi::Server &server, int from, int to, Value value, const std::vector<int> &card_indices)
{
    CardKnowledge::setServer(*this, server);
    assert(server.whoAmI() == me_);

    /* Someone has given Bob a value hint. If the named cards
     * include the one Bob would normally be discarding next,
     * then this must be a warning that that card is valuable.
     * Otherwise, all the named cards are playable. */

    const int playerExpectingWarning = (from + 1) % handKnowledge_.size();
    const int discardIndex = this->nextDiscardIndex(playerExpectingWarning);

    const bool isHintStoneReclaim =
        (!server.discardingIsAllowed()) &&
        (from == (to+1) % server.numPlayers()) &&
        vector_contains(card_indices, 0);
    const bool isWarning =
        !isHintStoneReclaim &&
        (to == playerExpectingWarning) &&
        vector_contains(card_indices, discardIndex) &&
        couldBeValuableWithValue(server, handKnowledge_[to][discardIndex], value);

    if (isWarning) {
        assert(discardIndex != -1);
        handKnowledge_[to][discardIndex].setIsValuable(*this, server, true);
    }

    if (to != playerExpectingWarning) {
        this->noValuableWarningWasGiven(server, from);
    }

    const int numCards = server.sizeOfHandOfPlayer(to);

    bool seenPlayable = false;
    for (int i=numCards-1; i >= 0; --i) {
        CardKnowledge &knol = handKnowledge_[to][i];
        if (vector_contains(card_indices, i)) {
            const bool wasMaybePlayable = (knol.playable() == MAYBE);
            knol.setMustBe(value);
            const bool isntUnplayable = (knol.playable() != NO);
            if (!isWarning && !isHintStoneReclaim && wasMaybePlayable && isntUnplayable && !seenPlayable) {
                seenPlayable = true;
                knol.setIsPlayable(server, true);
            }
        } else {
            knol.setCannotBe(value);
        }
    }
}

void SmartBot::pleaseObserveAfterMove(const Hanabi::Server &server)
{
    assert(server.whoAmI() == me_);
}

bool SmartBot::maybePlayLowestPlayableCard(Server &server)
{
    /* Try to find a card that nobody else knows I know is playable
     * (because they don't see what I see). Let's try to get that card
     * out of my hand before someone "helpfully" wastes a hint on it.
     * Otherwise, prefer lower-valued cards over higher-valued ones.
     */
    CardKnowledge eyeKnol[5];
    for (int i=0; i < myHandSize_; ++i) {
        eyeKnol[i] = handKnowledge_[me_][i];
        eyeKnol[i].update<true>();
    }

    int best_index = -1;
    int best_fitness = 0;
    for (int i=0; i < myHandSize_; ++i) {
        if (eyeKnol[i].playable() != YES) continue;

        /* How many further plays are enabled by this play?
         * Rough heuristic: 5 minus its value. Notice that this
         * gives an extra-high fitness to cards that are "known playable"
         * but whose color/value is unknown (value() == -1).
         * TODO: If both red 4s were discarded, then the red 3 doesn't open up any plays.
         * TODO: avoid stepping on other players' plays.
         */
        int fitness = (6 - eyeKnol[i].value());
        if (handKnowledge_[me_][i].playable() != YES) fitness += 100;
        if (fitness > best_fitness) {
            best_index = i;
            best_fitness = fitness;
        }
    }

    /* If I found a card to play, play it. */
    if (best_index != -1) {
        server.pleasePlay(best_index);
        return true;
    }

    return false;
}

bool SmartBot::maybeDiscardWorthlessCard(Server &server)
{
    /* Try to find a card that nobody else knows I know is worthless
     * (because they don't see what I see). Let's try to get that card
     * out of my hand before someone "helpfully" wastes a hint on it.
     */
    CardKnowledge eyeKnol[5];
    for (int i=0; i < myHandSize_; ++i) {
        eyeKnol[i] = handKnowledge_[me_][i];
        eyeKnol[i].update<true>();
    }

    int best_index = -1;
    int best_fitness = 0;
    for (int i=0; i < myHandSize_; ++i) {
        if (eyeKnol[i].worthless() != YES) continue;

        /* Prefer cards that I've deduced are worthless. */
        int fitness = (handKnowledge_[me_][i].worthless() == YES) ? 1 : 2;
        if (fitness > best_fitness) {
            best_index = i;
            best_fitness = fitness;
        }
    }

    /* If I found a card to discard, discard it. */
    if (best_index != -1) {
        server.pleaseDiscard(best_index);
        return true;
    }

    return false;
}

Hint SmartBot::bestHintForPlayer(const Server &server, int partner) const
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
        const int playableValue = server.pileOf(color).size() + 1;
        if (playableValue > 5) continue;  /* this pile is complete */
        int playability_content = 0;
        int color_content = 0;
        bool misinformative = false;
        bool seenPlayable = false;
        for (int c=partners_hand.size()-1; c >= 0; --c) {
            const CardKnowledge &knol = handKnowledge_[partner][c];
            if (partners_hand[c].color != color) continue;
            if (knol.playable() == MAYBE) {
                if (is_really_playable[c]) {
                    seenPlayable = true;
                    playability_content = 1;
                } else {
                    if (!seenPlayable && !knol.cannotBe(Card(color, playableValue))) {
                        misinformative = true;
                        break;
                    }
                }
            }
            color_content += (knol.color() == -1);
        }
        if (misinformative) continue;
        const int fitness = (playability_content == 0) ? 0 : (playability_content + color_content);
        if (fitness > best_so_far.fitness) {
            best_so_far.fitness = fitness;
            best_so_far.color = color;
            best_so_far.value = -1;
        }
    }

    /* Avoid giving hints that could be misinterpreted as warnings. */
    int valueToAvoid = -1;
    if (partner == (me_ + 1) % handKnowledge_.size()) {
        const int discardIndex = nextDiscardIndex(partner);
        if (discardIndex != -1) {
            const CardKnowledge &knol = handKnowledge_[partner][discardIndex];
            valueToAvoid = partners_hand[discardIndex].value;
            if (!couldBeValuableWithValue(server, knol, valueToAvoid)) valueToAvoid = -1;
        }
    }

    for (int value = 1; value <= 5; ++value) {
        if (value == valueToAvoid) continue;
        int playability_content = 0;
        int value_content = 0;
        bool misinformative = false;
        bool seenPlayable = false;
        for (int c=partners_hand.size()-1; c >= 0; --c) {
            const CardKnowledge &knol = handKnowledge_[partner][c];
            if (partners_hand[c].value != value) continue;
            if (knol.playable() == MAYBE) {
                if (is_really_playable[c]) {
                    seenPlayable = true;
                    playability_content = 1;
                } else {
                    if (!seenPlayable && couldBePlayableWithValue(server, knol, value)) {
                        misinformative = true;
                        break;
                    }
                }
            }
            value_content += (knol.value() == -1);
        }
        if (misinformative) continue;
        const int fitness = (playability_content == 0) ? 0 : (playability_content + value_content);
        if (fitness > best_so_far.fitness) {
            best_so_far.fitness = fitness;
            best_so_far.color = -1;
            best_so_far.value = value;
        }
    }

    return best_so_far;
}

bool SmartBot::maybeGiveValuableWarning(Server &server)
{
    /* Sometimes we just can't give a hint. */
    if (server.hintStonesRemaining() == 0) return false;

    const int numPlayers = handKnowledge_.size();
    const int player_to_warn = (me_ + 1) % numPlayers;

    /* Is the player to our left just about to discard a card
     * that is really valuable? */
    int discardIndex = this->nextDiscardIndex(player_to_warn);
    if (discardIndex == -1) return false;
    Card targetCard = server.handOfPlayer(player_to_warn)[discardIndex];
    if (!this->isValuable(server, targetCard)) {
        /* The target card isn't actually valuable. Good. */
        return false;
    }

    /* Oh no! Warn him before he discards it! */
    assert(handKnowledge_[player_to_warn][discardIndex].playable() != YES);
    assert(handKnowledge_[player_to_warn][discardIndex].valuable() != YES);
    assert(handKnowledge_[player_to_warn][discardIndex].worthless() != YES);

    Hint bestHint = bestHintForPlayer(server, player_to_warn);
    if (bestHint.fitness > 0) {
        /* Excellent; we found a hint that will cause him to play a card
         * instead of discarding. */
        bestHint.give(server);
        return true;
    }

    /* Otherwise, we'll have to give a warning. */
    server.pleaseGiveValueHint(player_to_warn, targetCard.value);
    return true;
}

bool SmartBot::maybeDiscardFinesse(Server &server)
{
    if (!server.discardingIsAllowed()) return false;

    std::vector<Card> myPlayableCards;
    std::vector<int> myPlayableIndices;

    for (int i = 0; i < handKnowledge_[me_].size(); ++i) {
        const CardKnowledge& knol = handKnowledge_[me_][i];
        if (knol.known() && knol.valuable() == NO && knol.playable() == YES) {
            myPlayableCards.push_back(knol.knownCard());
            myPlayableIndices.push_back(i);
        }
    }

    if (myPlayableCards.empty()) return false;

    std::vector<Card> othersNewestCards;

    const int numPlayers = handKnowledge_.size();
    for (int i = 1; i < numPlayers; ++i) {
        const int partner = (me_ + i) % numPlayers;
        othersNewestCards.push_back(server.handOfPlayer(partner).back());
    }

    for (int j = 0; j < myPlayableCards.size(); ++j) {
        if (vector_count(othersNewestCards, myPlayableCards[j]) == 1) {
            server.pleaseDiscard(myPlayableIndices[j]);
            return true;
        }
    }

    return false;
}

bool SmartBot::maybeGiveHelpfulHint(Server &server)
{
    if (server.hintStonesRemaining() == 0) return false;

    const int numPlayers = handKnowledge_.size();
    Hint bestHint;
    for (int i = 1; i < numPlayers; ++i) {
        const int partner = (me_ + i) % numPlayers;
        Hint candidate = bestHintForPlayer(server, partner);
        if (candidate.fitness > bestHint.fitness) {
            bestHint = candidate;
        }
    }

    if (bestHint.fitness <= 0) return false;

    /* Give the hint. */
    bestHint.give(server);
    return true;
}

bool SmartBot::maybePlayMysteryCard(Server &server)
{
    if (!UseMulligans) return false;

    const int table[4] = { -99, 1, 1, 3 };
    if (server.cardsRemainingInDeck() <= table[server.mulligansRemaining()]) {
        /* We could temporize, or we could do something that forces us to
         * draw a card. If we got here, temporizing has been rejected as
         * an option; so let's do something that forces us to draw a card.
         * At this point, we might as well try to play something random
         * and hope we get lucky. */
        double best_fitness = 0;
        int best_index = -1;
        for (int i = handKnowledge_[me_].size() - 1; i >= 0; --i) {
            CardKnowledge eyeKnol = handKnowledge_[me_][i];
            eyeKnol.update<true>();
            assert(eyeKnol.playable() != YES);  /* or we would have played it already */
            if (eyeKnol.playable() == MAYBE) {
                double fitness = eyeKnol.probabilityPlayable();
                if (fitness > best_fitness) {
                    best_fitness = fitness;
                    best_index = i;
                }
            }
        }
        if (best_index != -1) {
            server.pleasePlay(best_index);
            return true;
        }
    }
    return false;
}

bool SmartBot::maybeDiscardOldCard(Server &server)
{
    const int best_index = nextDiscardIndex(me_);
    if (best_index != -1) {
        server.pleaseDiscard(best_index);
        return true;
    }
    /* I didn't find anything I was willing to discard. */
    return false;
}

void SmartBot::pleaseMakeMove(Server &server)
{
    CardKnowledge::setServer(*this, server);
    assert(server.whoAmI() == me_);
    assert(server.activePlayer() == me_);
    assert(UseMulligans || !server.mulligansUsed());

    if (server.cardsRemainingInDeck() == 0) {
        if (maybePlayLowestPlayableCard(server)) return;
        if (maybePlayMysteryCard(server)) return;
    }
    if (maybeGiveValuableWarning(server)) return;
    if (maybeDiscardFinesse(server)) return;
    if (maybePlayLowestPlayableCard(server)) return;
    if (maybeGiveHelpfulHint(server)) return;
    if (maybePlayMysteryCard(server)) return;

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
            assert(handKnowledge_[me_][i].valuable() == YES);
            if (handKnowledge_[me_][i].value() > handKnowledge_[me_][best_index].value()) {
                best_index = i;
            }
        }
        server.pleaseDiscard(best_index);
    }
}
