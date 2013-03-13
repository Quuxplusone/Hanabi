
#include <algorithm>
#include <cassert>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Hanabi.h"

static std::string nth(int n)
{
    switch (n) {
        case 0: return "oldest";
        case 1: return "second-oldest";
        case 2: return "second-newest";
        case 3: return "newest";
    }
    assert(false);
}

static std::string nth(const std::vector<int> &ns)
{
    std::string result;
    assert(!ns.empty());
    switch (ns.size()) {
        case 1: return nth(ns[0]);
        case 2: return nth(ns[0]) + " and " + nth(ns[1]);
        case 3: return nth(ns[0]) + ", " + nth(ns[1]) + ", and " + nth(ns[2]);
    }
    assert(false);
}

static std::string colorname(Hanabi::Color color)
{
    switch (color) {
        case Hanabi::RED: return "red";
        case Hanabi::ORANGE: return "orange";
        case Hanabi::YELLOW: return "yellow";
        case Hanabi::GREEN: return "green";
        case Hanabi::BLUE: return "blue";
    }
    assert(false);
}

namespace Hanabi {

Card::Card(Color c, Value v): color(c), value(v) { }
Card::Card(Color c, int v): color(c), value(Value(v)) { }

int Card::count() const
{
    switch (value) {
        case 1: return 3;
        case 2: case 3: case 4: return 2;
        case 5: return 1;
        default: throw std::runtime_error("invalid card value");
    }
}

std::string Card::toString() const
{
    std::string result;
    result += (char)('0' + this->value);
    result += (char)(colorname(this->color)[0]);
    return result;
}

bool Card::operator== (const Card &rhs) const
{
    return (this->color == rhs.color) && (this->value == rhs.value);
}

bool Card::operator!= (const Card &rhs) const
{
    return !(*this == rhs);
}

bool Pile::empty() const
{
    return (value == 0);
}

Card Pile::topCard() const
{
    if (value == 0) {
        throw std::runtime_error("empty pile has no top card");
    } else if (value < 0 || 5 < value) {
        throw std::logic_error("top card of pile is invalid");
    } else {
        return Card(color, value);
    }
}

bool Pile::nextValueIs(Value v) const
{
    return (v == this->value + 1);
}

void Pile::increment_()
{
    assert(0 <= value && value <= 4);
    ++value;
}

/* virtual destructor */
Bot::~Bot() { }

/* Hanabi::Card has no default constructor */
Server::Server(): activeCard_(RED,1) { }

bool Server::gameOver() const
{
    /* The game ends if there are no more cards to draw... */
    if (this->deck_.empty()) return true;
    /* ...no more mulligans available... */
    if (this->mulligansRemaining_ == 0) return true;
    /* ...or if the piles are all complete. */
    if (this->currentScore() == 5*NUMCOLORS) return true;
    /* Otherwise, the game has not ended. */
    return false;
}

int Server::currentScore() const
{
    int sum = 0;
    for (int color = 0; color < NUMCOLORS; ++color) {
        const Pile &pile = this->piles_[color];
        if (!pile.empty()) {
            sum += pile.topCard().value;
        }
    }
    return sum;
}

void Server::setLog(std::ostream *logStream)
{
    this->log_ = logStream;
}

int Server::runGame(const BotFactory &botFactory, int numPlayers)
{
    /* Create and initialize the bots. */
    players_.resize(numPlayers);
    for (int i=0; i < numPlayers; ++i) {
        players_[i] = botFactory.create(i, numPlayers);
    }

    /* Initialize the piles and stones. */
    for (Color color = RED; color <= BLUE; ++color) {
        piles_[(int)color].color = color;
        piles_[(int)color].value = 0;
    }
    mulligansRemaining_ = NUMMULLIGANS;
    hintStonesRemaining_ = NUMHINTS;

    /* Shuffle the deck. */
    deck_.clear();
    for (Color color = RED; color <= BLUE; ++color) {
        for (int value = 1; value <= 5; ++value) {
            const Card card(color, value);
            const int n = card.count();
            for (int k=0; k < n; ++k) deck_.push_back(card);
        }
    }
    std::random_shuffle(deck_.begin(), deck_.end());

    /* Secretly draw the starting hands. */
    hands_.resize(numPlayers);
    for (int i=0; i < numPlayers; ++i) {
        hands_[i].clear();
        for (int k=0; k < 4; ++k) {
            hands_[i].push_back(this->draw_());
        }
    }

    /* Run the game. */
    activeCardIsObservable_ = false;
    activePlayer_ = 0;
    while (!this->gameOver()) {
        if (activePlayer_ == 0) this->logHands_();
        activePlayerHasMoved_ = false;
        for (int i=0; i < numPlayers; ++i) {
            observingPlayer_ = i;
            players_[i]->pleaseObserveBeforeMove(*this);
        }
        observingPlayer_ = activePlayer_;
        assert(!activePlayerHasMoved_);
        players_[activePlayer_]->pleaseMakeMove(*this);  /* make a move */
        if (!activePlayerHasMoved_) {
            throw std::runtime_error("bot failed to respond to pleaseMove()");
        }
        for (int i=0; i < numPlayers; ++i) {
            observingPlayer_ = i;
            players_[i]->pleaseObserveAfterMove(*this);
        }
        activePlayer_ = (activePlayer_ + 1) % numPlayers;
    }

    for (int i=0; i < numPlayers; ++i) {
        botFactory.destroy(players_[i]);
    }

    return this->currentScore();
}

int Server::whoAmI() const
{
    assert(0 <= observingPlayer_ && observingPlayer_ < players_.size());
    return observingPlayer_;
}

int Server::activePlayer() const
{
    return activePlayer_;
}

std::vector<Card> Server::handOfPlayer(int player) const
{
    if (player == observingPlayer_) throw std::runtime_error("cannot observe own hand");
    if (player < 0 || hands_.size() <= player) throw std::runtime_error("player index out of bounds");
    return hands_[player];
}

Card Server::activeCard() const
{
    if (!activeCardIsObservable_) throw std::runtime_error("called activeCard() from the wrong observer");
    return activeCard_;
}

Pile Server::pileOf(Color color) const
{
    int index = (int)color;
    if (index < 0 || NUMCOLORS <= index) throw std::runtime_error("invalid Color");
    return piles_[color];
}

std::vector<Card> Server::discards() const
{
    return discards_;
}

int Server::hintStonesUsed() const
{
    assert(hintStonesRemaining_ <= NUMHINTS);
    return (NUMHINTS - hintStonesRemaining_);
}

int Server::hintStonesRemaining() const
{
    assert(hintStonesRemaining_ <= NUMHINTS);
    return hintStonesRemaining_;
}

int Server::mulligansUsed() const
{
    assert(mulligansRemaining_ <= NUMMULLIGANS);
    return (NUMMULLIGANS - mulligansRemaining_);
}

int Server::mulligansRemaining() const
{
    assert(mulligansRemaining_ <= NUMMULLIGANS);
    return mulligansRemaining_;
}

int Server::cardsRemainingInDeck() const
{
    return deck_.size();
}

Card Server::pleaseDiscard(int index)
{
    assert(0 <= activePlayer_ && activePlayer_ < hands_.size());
    if (activePlayerHasMoved_) throw std::runtime_error("bot attempted to move twice");
    if (index < 0 || hands_[activePlayer_].size() <= index) throw std::runtime_error("invalid card index");

    Card discardedCard = hands_[activePlayer_][index];
    activeCard_ = discardedCard;
    activeCardIsObservable_ = true;

    /* Notify all the players of the discard (before it happens). */
    int oldObservingPlayer = observingPlayer_;
    for (int i=0; i < players_.size(); ++i) {
        observingPlayer_ = i;
        players_[i]->pleaseObserveBeforeDiscard(*this, activePlayer_, index);
    }
    observingPlayer_ = oldObservingPlayer;
    activeCardIsObservable_ = false;

    /* Discard the selected card. */
    discards_.push_back(discardedCard);

    /* Shift the old cards down, and draw a replacement. */
    for (int i = index; i+1 < hands_[activePlayer_].size(); ++i) {
        hands_[activePlayer_][i] = hands_[activePlayer_][i+1];
    }
    Card replacementCard = this->draw_();
    hands_[activePlayer_].back() = replacementCard;

    /* Regain a hint-stone, if possible. */
    bool regainedHintStone = false;
    if (hintStonesRemaining_ < NUMHINTS) {
        ++hintStonesRemaining_;
        regainedHintStone = true;
    }

    activePlayerHasMoved_ = true;

    if (log_) {
        (*log_) << "Player " << activePlayer_
                << " discarded his " << nth(index)
                << " card (" << discardedCard.toString() << ").\n";
        (*log_) << "Player " << activePlayer_
                << " drew a replacement (" << replacementCard.toString() << ").\n";
        if (regainedHintStone) {
            (*log_) << "Player " << activePlayer_
                    << " returned a hint stone; there "
                    << ((hintStonesRemaining_ == 1) ? "is" : "are") << " now "
                    << hintStonesRemaining_ << " remaining.\n";
        }
    }

    return discardedCard;
}

Card Server::pleasePlay(int index)
{
    assert(0 <= activePlayer_ && activePlayer_ < hands_.size());
    assert(players_.size() == hands_.size());
    if (activePlayerHasMoved_) throw std::runtime_error("bot attempted to move twice");
    if (index < 0 || hands_[activePlayer_].size() <= index) throw std::runtime_error("invalid card index");

    Card selectedCard = hands_[activePlayer_][index];
    activeCard_ = selectedCard;
    activeCardIsObservable_ = true;

    /* Notify all the players of the attempted play (before it happens). */
    int oldObservingPlayer = observingPlayer_;
    for (int i=0; i < players_.size(); ++i) {
        observingPlayer_ = i;
        players_[i]->pleaseObserveBeforePlay(*this, activePlayer_, index);
    }
    observingPlayer_ = oldObservingPlayer;
    activeCardIsObservable_ = false;

    /* Examine the selected card. */
    Pile &pile = piles_[(int)selectedCard.color];
    bool success = false;

    if (pile.nextValueIs(selectedCard.value)) {
        pile.increment_();
        success = true;
    } else {
        /* The card was unplayable! */
        discards_.push_back(selectedCard);
        --mulligansRemaining_;
        assert(mulligansRemaining_ >= 0);
    }

    /* Shift the old cards down, and draw a replacement. */
    for (int i = index; i+1 < hands_[activePlayer_].size(); ++i) {
        hands_[activePlayer_][i] = hands_[activePlayer_][i+1];
    }
    Card replacementCard = this->draw_();
    hands_[activePlayer_].back() = replacementCard;

    activePlayerHasMoved_ = true;

    if (log_) {
        if (success) {
            (*log_) << "Player " << activePlayer_
                    << " played his " << nth(index)
                    << " card (" << selectedCard.toString() << ").\n";
        } else {
            const bool one = (mulligansRemaining_ == 1);
            const bool none = (mulligansRemaining_ == 0);
            (*log_) << "Player " << activePlayer_
                    << " tried to play his " << nth(index)
                    << " card (" << selectedCard.toString() << ")"
                    << " but failed.\n";
            if (mulligansRemaining_ == 0) {
                (*log_) << "That was the last mulligan.\n";
            } else if (mulligansRemaining_ == 1) {
                (*log_) << "There is only one mulligan remaining.\n";
            } else {
                (*log_) << "There are " << mulligansRemaining_ << " mulligans remaining.\n";
            }
        }
        if (mulligansRemaining_ != 0) {
            (*log_) << "Player " << activePlayer_
                    << " drew a replacement (" << replacementCard.toString() << ").\n";
        }
    }

    return selectedCard;
}

void Server::pleaseGiveColorHint(int to, Color color)
{
    assert(0 <= activePlayer_ && activePlayer_ < hands_.size());
    assert(players_.size() == hands_.size());
    if (activePlayerHasMoved_) throw std::runtime_error("bot attempted to move twice");
    if (to < 0 || hands_.size() <= to) throw std::runtime_error("invalid player index");
    if (color < RED || BLUE < color) throw std::runtime_error("invalid color");
    if (hintStonesRemaining_ == 0) throw std::runtime_error("no hint stones remaining");
    if (to == activePlayer_) throw std::runtime_error("cannot give hint to oneself");

    std::vector<int> card_indices;
    for (int i=0; i < hands_[to].size(); ++i) {
        if (hands_[to][i].color == color) {
            card_indices.push_back(i);
        }
    }
    if (card_indices.empty()) throw std::runtime_error("hint must include at least one card");

    if (log_) {
        const bool singular = (card_indices.size() == 1);
        (*log_) << "Player " << activePlayer_
                << " told player " << to
                << " that his ";
        if (card_indices.size() == hands_[to].size()) {
            (*log_) << "whole hand was ";
        } else {
            (*log_) << nth(card_indices)
                << (singular ? " card was " : " cards were ");
        }
        (*log_) << colorname(color) << ".\n";
    }

    /* Notify all the players of the given hint. */
    int oldObservingPlayer = observingPlayer_;
    for (int i=0; i < players_.size(); ++i) {
        observingPlayer_ = i;
        players_[i]->pleaseObserveColorHint(*this, activePlayer_, to, color, card_indices);
    }
    observingPlayer_ = oldObservingPlayer;

    hintStonesRemaining_ -= 1;
    activePlayerHasMoved_ = true;
}

void Server::pleaseGiveValueHint(int to, Value value)
{
    assert(0 <= activePlayer_ && activePlayer_ < hands_.size());
    assert(players_.size() == hands_.size());
    if (activePlayerHasMoved_) throw std::runtime_error("bot attempted to move twice");
    if (to < 0 || players_.size() <= to) throw std::runtime_error("invalid player index");
    if (value <= 0 || 5 < value) throw std::runtime_error("invalid value");
    if (hintStonesRemaining_ == 0) throw std::runtime_error("no hint stones remaining");
    if (to == activePlayer_) throw std::runtime_error("cannot give hint to oneself");

    std::vector<int> card_indices;
    for (int i=0; i < hands_[to].size(); ++i) {
        if (hands_[to][i].value == value) {
            card_indices.push_back(i);
        }
    }
    if (card_indices.empty()) throw std::runtime_error("hint must include at least one card");

    if (log_) {
        const bool singular = (card_indices.size() == 1);
        (*log_) << "Player " << activePlayer_
                << " told player " << to
                << " that his ";
        if (card_indices.size() == hands_[to].size()) {
            (*log_) << "whole hand was ";
        } else {
            (*log_) << nth(card_indices)
                << (singular ? " card was " : " cards were ");
        }
        (*log_) << value
                << (singular ? ".\n" : "s.\n");
    }

    /* Notify all the players of the given hint. */
    int oldObservingPlayer = observingPlayer_;
    for (int i=0; i < players_.size(); ++i) {
        observingPlayer_ = i;
        players_[i]->pleaseObserveValueHint(*this, activePlayer_, to, value, card_indices);
    }
    observingPlayer_ = oldObservingPlayer;

    hintStonesRemaining_ -= 1;
    activePlayerHasMoved_ = true;
}

Card Server::draw_()
{
    assert(!deck_.empty());
    Card result = deck_.back();
    deck_.pop_back();
    return result;
}

void Server::logHands_()
{
    if (log_) {
        (*log_) << "Current hands:";
        for (int i=0; i < hands_.size(); ++i) {
            assert(hands_[i].size() == 4);
            (*log_) << " " << hands_[i][0].toString()
                    << "," << hands_[i][1].toString()
                    << "," << hands_[i][2].toString()
                    << "," << hands_[i][3].toString();
        }
        (*log_) << "\n";
    }
}

}  /* namespace Hanabi */
