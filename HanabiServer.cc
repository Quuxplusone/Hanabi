
#include <algorithm>
#include <cassert>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Hanabi.h"

static std::string nth(int n, int total)
{
    if (total == 5) {
        switch (n) {
            case 0: return "oldest";
            case 1: return "second-oldest";
            case 2: return "middle";
            case 3: return "second-newest";
            default: assert(n == 4); return "newest";
        }
    } else if (total == 4) {
        switch (n) {
            case 0: return "oldest";
            case 1: return "second-oldest";
            case 2: return "second-newest";
            default: assert(n == 3); return "newest";
        }
    } else {
        assert(total == 3);
        switch (n) {
            case 0: return "oldest";
            case 1: return "middle";
            default: assert(n == 2); return "newest";
        }
    }
}

static std::string nth(const std::vector<int> &ns, int total)
{
    std::string result;
    assert(!ns.empty());
    switch (ns.size()) {
        case 1: return nth(ns[0], total);
        case 2: return nth(ns[0], total) + " and " + nth(ns[1], total);
        case 3: return nth(ns[0], total) + ", " + nth(ns[1], total) + ", and " + nth(ns[2], total);
        default:
            assert(ns.size() == 4);
            return nth(ns[0], total) + ", " + nth(ns[1], total) + ", " + nth(ns[2], total) + ", and " + nth(ns[3], total);
    }
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

Card::Card(Color c, Value v): color(c), value(v) { assert(1 <= value && value <= 5); }
Card::Card(Color c, int v): color(c), value(Value(v)) { assert(1 <= value && value <= 5); }

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

Card Pile::topCard() const
{
    if (size_ == 0) {
        throw std::runtime_error("empty pile has no top card");
    } else {
        assert(1 <= size_ && size_ <= 5);
        return Card(color, size_);
    }
}

void Pile::increment_()
{
    assert(0 <= size_ && size_ <= 4);
    ++size_;
}

/* virtual destructor */
Bot::~Bot() { }

/* Hanabi::Card has no default constructor */
Server::Server(): activeCard_(RED,1), log_(NULL) { }

bool Server::gameOver() const
{
    /* The game ends if there are no more cards to draw... */
    if (this->deck_.empty() && finalCountdown_ == numPlayers_+1) return true;
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
    return this->runGame(botFactory, numPlayers, std::vector<Card>());
}

int Server::runGame(const BotFactory &botFactory, int numPlayers, const std::vector<Card>& stackedDeck)
{
    const int initialHandSize = (numPlayers <= 3) ? 5 : 4;

    /* Create and initialize the bots. */
    numPlayers_ = numPlayers;
    players_.resize(numPlayers);
    for (int i=0; i < numPlayers; ++i) {
        players_[i] = botFactory.create(i, numPlayers, initialHandSize);
    }

    /* Initialize the piles and stones. */
    for (Color color = RED; color <= BLUE; ++color) {
        piles_[(int)color].color = color;
        piles_[(int)color].size_ = 0;
    }
    mulligansRemaining_ = NUMMULLIGANS;
    hintStonesRemaining_ = NUMHINTS;
    finalCountdown_ = 0;

    /* Shuffle the deck. */
    if (!stackedDeck.empty()) {
        deck_ = stackedDeck;
        std::reverse(deck_.begin(), deck_.end());  /* because we pull cards from the "top" (back) of the vector */
    } else {
        deck_.clear();
        for (Color color = RED; color <= BLUE; ++color) {
            for (int value = 1; value <= 5; ++value) {
                const Card card(color, value);
                const int n = card.count();
                for (int k=0; k < n; ++k) deck_.push_back(card);
            }
        }
        std::random_shuffle(deck_.begin(), deck_.end());
    }
    discards_.clear();

    /* Secretly draw the starting hands. */
    hands_.resize(numPlayers);
    for (int i=0; i < numPlayers; ++i) {
        hands_[i].clear();
        for (int k=0; k < initialHandSize; ++k) {
            hands_[i].push_back(this->draw_());
        }
    }

    /* Run the game. */
    activeCardIsObservable_ = false;
    activePlayer_ = 0;
    movesFromActivePlayer_ = -1;
    while (!this->gameOver()) {
        if (activePlayer_ == 0) this->logHands_();
        for (int i=0; i < numPlayers; ++i) {
            observingPlayer_ = i;
            players_[i]->pleaseObserveBeforeMove(*this);
        }
        observingPlayer_ = activePlayer_;
        movesFromActivePlayer_ = 0;
        players_[activePlayer_]->pleaseMakeMove(*this);  /* make a move */
        if (movesFromActivePlayer_ == 0) {
            throw std::runtime_error("bot failed to respond to pleaseMove()");
        }
        assert(movesFromActivePlayer_ == 1);
        movesFromActivePlayer_ = -1;
        for (int i=0; i < numPlayers; ++i) {
            observingPlayer_ = i;
            players_[i]->pleaseObserveAfterMove(*this);
        }
        activePlayer_ = (activePlayer_ + 1) % numPlayers;
        assert(0 <= finalCountdown_ && finalCountdown_ <= numPlayers);
        if (deck_.empty()) finalCountdown_ += 1;
    }

    for (int i=0; i < numPlayers; ++i) {
        botFactory.destroy(players_[i]);
    }

    return this->currentScore();
}

int Server::numPlayers() const
{
    return numPlayers_;
}

int Server::whoAmI() const
{
    assert(0 <= observingPlayer_ && observingPlayer_ < numPlayers_);
    return observingPlayer_;
}

int Server::activePlayer() const
{
    return activePlayer_;
}

int Server::sizeOfHandOfPlayer(int player) const
{
    if (player < 0 || numPlayers_ <= player) throw std::runtime_error("player index out of bounds");
    return hands_[player].size();
}

std::vector<Card> Server::handOfPlayer(int player) const
{
    if (player == observingPlayer_) throw std::runtime_error("cannot observe own hand");
    if (player < 0 || numPlayers_ <= player) throw std::runtime_error("player index out of bounds");
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

bool Server::discardingIsAllowed() const
{
    return (hintStonesRemaining_ != NUMHINTS);
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
    assert(0 <= activePlayer_ && activePlayer_ < numPlayers_);
    if (movesFromActivePlayer_ > 0) throw std::runtime_error("bot attempted to move twice");
    if (movesFromActivePlayer_ < 0) throw std::runtime_error("called pleaseDiscard() from the wrong observer");
    if (index < 0 || hands_[activePlayer_].size() <= index) throw std::runtime_error("invalid card index");
    if (!discardingIsAllowed()) throw std::runtime_error("all hint stones are already available");

    Card discardedCard = hands_[activePlayer_][index];
    activeCard_ = discardedCard;
    activeCardIsObservable_ = true;

    /* Notify all the players of the discard (before it happens). */
    movesFromActivePlayer_ = -1;
    int oldObservingPlayer = observingPlayer_;
    for (int i=0; i < numPlayers_; ++i) {
        observingPlayer_ = i;
        players_[i]->pleaseObserveBeforeDiscard(*this, activePlayer_, index);
    }
    observingPlayer_ = oldObservingPlayer;
    activeCardIsObservable_ = false;

    /* Discard the selected card. */
    discards_.push_back(discardedCard);

    if (log_) {
        (*log_) << "Player " << activePlayer_
                << " discarded his " << nth(index, hands_[activePlayer_].size())
                << " card (" << discardedCard.toString() << ").\n";
    }

    /* Shift the old cards down, and draw a replacement if possible. */
    hands_[activePlayer_].erase(hands_[activePlayer_].begin() + index);

    if (mulligansRemaining_ > 0 && !deck_.empty()) {
        Card replacementCard = this->draw_();
        hands_[activePlayer_].push_back(replacementCard);
        if (log_) {
            (*log_) << "Player " << activePlayer_
                    << " drew a replacement (" << replacementCard.toString() << ").\n";
        }
    }

    regainHintStoneIfPossible_();
    movesFromActivePlayer_ = 1;

    return discardedCard;
}

Card Server::pleasePlay(int index)
{
    assert(0 <= activePlayer_ && activePlayer_ < hands_.size());
    assert(players_.size() == hands_.size());
    if (movesFromActivePlayer_ > 0) throw std::runtime_error("bot attempted to move twice");
    if (movesFromActivePlayer_ < 0) throw std::runtime_error("called pleasePlay() from the wrong observer");
    if (index < 0 || hands_[activePlayer_].size() <= index) throw std::runtime_error("invalid card index");

    Card selectedCard = hands_[activePlayer_][index];
    activeCard_ = selectedCard;
    activeCardIsObservable_ = true;

    /* Notify all the players of the attempted play (before it happens). */
    movesFromActivePlayer_ = -1;
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
        if (log_) {
            (*log_) << "Player " << activePlayer_
                    << " played his " << nth(index, hands_[activePlayer_].size())
                    << " card (" << selectedCard.toString() << ").\n";
        }
        pile.increment_();
        if (selectedCard.value == 5) {
            /* Successfully playing a 5 regains a hint stone. */
            regainHintStoneIfPossible_();
        }
        success = true;
    } else {
        /* The card was unplayable! */
        if (log_) {
            (*log_) << "Player " << activePlayer_
                    << " tried to play his " << nth(index, hands_[activePlayer_].size())
                    << " card (" << selectedCard.toString() << ")"
                    << " but failed.\n";
        }
        discards_.push_back(selectedCard);
        loseMulligan_();
    }

    /* Shift the old cards down, and draw a replacement if possible. */
    hands_[activePlayer_].erase(hands_[activePlayer_].begin() + index);

    if (mulligansRemaining_ > 0 && !deck_.empty()) {
        Card replacementCard = this->draw_();
        hands_[activePlayer_].push_back(replacementCard);
        if (log_) {
            (*log_) << "Player " << activePlayer_
                    << " drew a replacement (" << replacementCard.toString() << ").\n";
        }
    }

    this->logPiles_();

    movesFromActivePlayer_ = 1;
    return selectedCard;
}

void Server::pleaseGiveColorHint(int to, Color color)
{
    assert(0 <= activePlayer_ && activePlayer_ < hands_.size());
    assert(players_.size() == hands_.size());
    if (movesFromActivePlayer_ > 0) throw std::runtime_error("bot attempted to move twice");
    if (movesFromActivePlayer_ < 0) throw std::runtime_error("called pleaseGiveColorHint() from the wrong observer");
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
    //if (card_indices.empty()) throw std::runtime_error("hint must include at least one card");

    if (log_) {
        const bool singular = (card_indices.size() == 1);
        (*log_) << "Player " << activePlayer_
                << " told player " << to
                << " that his ";
        if (card_indices.size() == hands_[to].size()) {
            (*log_) << "whole hand was ";
        } else if (card_indices.empty()) {
            (*log_) << "hand had no ";
        } else {
            (*log_) << nth(card_indices, hands_[to].size())
                << (singular ? " card was " : " cards were ");
        }
        (*log_) << colorname(color) << ".\n";
    }

    /* Notify all the players of the given hint. */
    movesFromActivePlayer_ = -1;
    int oldObservingPlayer = observingPlayer_;
    for (int i=0; i < players_.size(); ++i) {
        observingPlayer_ = i;
        players_[i]->pleaseObserveColorHint(*this, activePlayer_, to, color, card_indices);
    }
    observingPlayer_ = oldObservingPlayer;

    hintStonesRemaining_ -= 1;
    movesFromActivePlayer_ = 1;
}

void Server::pleaseGiveValueHint(int to, Value value)
{
    assert(0 <= activePlayer_ && activePlayer_ < hands_.size());
    assert(players_.size() == hands_.size());
    if (movesFromActivePlayer_ > 0) throw std::runtime_error("bot attempted to move twice");
    if (movesFromActivePlayer_ < 0) throw std::runtime_error("called pleaseGiveValueHint() from the wrong observer");
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
    //if (card_indices.empty()) throw std::runtime_error("hint must include at least one card");

    if (log_) {
        const bool singular = (card_indices.size() == 1);
        (*log_) << "Player " << activePlayer_
                << " told player " << to
                << " that his ";
        if (card_indices.size() == hands_[to].size()) {
            (*log_) << "whole hand was ";
        } else if (card_indices.empty()) {
            (*log_) << "hand had no ";
        } else {
            (*log_) << nth(card_indices, hands_[to].size())
                << (singular ? " card was " : " cards were ");
        }
        (*log_) << value
                << (singular ? ".\n" : "s.\n");
    }

    /* Notify all the players of the given hint. */
    movesFromActivePlayer_ = -1;
    int oldObservingPlayer = observingPlayer_;
    for (int i=0; i < players_.size(); ++i) {
        observingPlayer_ = i;
        players_[i]->pleaseObserveValueHint(*this, activePlayer_, to, value, card_indices);
    }
    observingPlayer_ = oldObservingPlayer;

    hintStonesRemaining_ -= 1;
    movesFromActivePlayer_ = 1;
}

void Server::regainHintStoneIfPossible_()
{
    if (hintStonesRemaining_ < NUMHINTS) {
        ++hintStonesRemaining_;
        if (log_) {
            (*log_) << "Player " << activePlayer_
                    << " returned a hint stone; there "
                    << ((hintStonesRemaining_ == 1) ? "is" : "are") << " now "
                    << hintStonesRemaining_ << " remaining.\n";
        }
    }
}

void Server::loseMulligan_()
{
    --mulligansRemaining_;
    assert(mulligansRemaining_ >= 0);
    if (log_) {
        if (mulligansRemaining_ == 0) {
            (*log_) << "That was the last mulligan.\n";
        } else if (mulligansRemaining_ == 1) {
            (*log_) << "There is only one mulligan remaining.\n";
        } else {
            (*log_) << "There are " << mulligansRemaining_ << " mulligans remaining.\n";
        }
    }
}

Card Server::draw_()
{
    assert(!deck_.empty());
    Card result = deck_.back();
    deck_.pop_back();
    return result;
}

void Server::logHands_() const
{
    if (log_) {
        (*log_) << "Current hands:";
        for (int i=0; i < numPlayers_; ++i) {
            for (int j=0; j < (int)hands_[i].size(); ++j) {
                (*log_) << (j ? "," : " ") << hands_[i][j].toString();
            }
        }
        (*log_) << "\n";
    }
}

void Server::logPiles_() const
{
    if (log_) {
        (*log_) << "Current piles:";
        for (Color k = RED; k <= BLUE; ++k) {
            (*log_) << " " << piles_[k].size() << Card(k, 1).toString()[1];
        }
        (*log_) << "\n";
    }
}

}  /* namespace Hanabi */
