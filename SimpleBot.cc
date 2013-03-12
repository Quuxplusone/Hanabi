
#include <cassert>
#include <cstdlib>
#include <iostream>
#include "Hanabi.h"
#include "SimpleBot.h"

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
}

bool CardKnowledge::mustBe(Hanabi::Color color) const { return (colors_[color] == YES); }
bool CardKnowledge::mustBe(Hanabi::Value value) const { return (values_[value] == YES); }

void CardKnowledge::setMustBe(Hanabi::Color color)
{
    assert(colors_[color] != NO);
    colors_[color] = YES;
}

void CardKnowledge::setMustBe(Hanabi::Value value)
{
    assert(values_[value] != NO);
    values_[value] = YES;
}


SimpleBot::SimpleBot(int index, int numPlayers)
{
    me_ = index;
    handKnowledge_.resize(numPlayers);
    for (int i=0; i < numPlayers; ++i) {
        handKnowledge_[i].resize(4);
    }
}

void SimpleBot::invalidateKnol(int player_index, int card_index)
{
    /* The other cards are shifted down and a new one drawn at the end. */
    std::vector<CardKnowledge> &vec = handKnowledge_[player_index];
    assert(vec.size() == 4);
    for (int i = card_index; i+1 < vec.size(); ++i) {
        vec[i] = vec[i+1];
    }
    vec.back() = CardKnowledge();
}

void SimpleBot::pleaseObserveBeforeMove(const Server &server)
{
    assert(server.whoAmI() == me_);
    this->needsToPostObserve_ = false;
}

void SimpleBot::pleaseObserveBeforeDiscard(const Hanabi::Server &server, int from, int card_index)
{
    assert(server.whoAmI() == me_);
    assert(server.activePlayer() != me_);
    this->invalidateKnol(from, card_index);
}

void SimpleBot::pleaseObserveBeforePlay(const Hanabi::Server &server, int from, int card_index)
{
    assert(server.whoAmI() == me_);
    assert(server.activePlayer() != me_);

    this->invalidateKnol(from, card_index);

    Card card = server.handOfPlayer(from)[card_index];
    this->wipeOutPlayables(card);
}

void SimpleBot::pleaseObserveColorHint(const Hanabi::Server &server, int from, int to, Color color, std::vector<int> card_indices)
{
    assert(server.whoAmI() == me_);
    assert(server.activePlayer() != me_);

    /* Someone has given P a color hint. Using SimpleBot's strategy,
     * this means that all the named cards are playable. */

    for (int i=0; i < card_indices.size(); ++i) {
        CardKnowledge &knol = handKnowledge_[to][card_indices[i]];
        knol.setMustBe(color);
        knol.isPlayable = true;
    }
}

void SimpleBot::pleaseObserveValueHint(const Hanabi::Server &server, int from, int to, Value value, std::vector<int> card_indices)
{
    assert(server.whoAmI() == me_);
    assert(server.activePlayer() != me_);

    /* Someone has given P a value hint. Using SimpleBot's strategy,
     * this means that all the named cards are playable. */

    for (int i=0; i < card_indices.size(); ++i) {
        CardKnowledge &knol = handKnowledge_[to][card_indices[i]];
        knol.setMustBe(value);
        knol.isPlayable = true;
    }
}

void SimpleBot::prepareToPostObserve(const Hanabi::Server &server)
{
    assert(server.whoAmI() == me_);
    assert(server.activePlayer() == me_);

    for (Color color = RED; color <= BLUE; ++color) {
        this->postObservedPiles_[color] = server.pileOf(color);
    }
    this->needsToPostObserve_ = true;
}

void SimpleBot::pleaseObserveAfterMove(const Hanabi::Server &server)
{
    if (!needsToPostObserve_) return;

    assert(server.whoAmI() == me_);
    assert(server.activePlayer() == me_);

    bool foundModifiedPile = false;
    for (Color color = RED; color <= BLUE; ++color) {
        Pile &original = this->postObservedPiles_[color];
        Pile modified = server.pileOf(color);

        if (modified.empty()) continue;
        if (original.nextValueIs(modified.topCard().value)) {
            /* This pile was added to by our move! */
            assert(!foundModifiedPile);
            this->wipeOutPlayables(modified.topCard());
            foundModifiedPile = true;
        }
    }

    this->needsToPostObserve_ = false;
}

void SimpleBot::wipeOutPlayables(const Card &played_card)
{
    const int numPlayers = handKnowledge_.size();
    for (int player = 0; player < numPlayers; ++player) {
        for (int c = 0; c < 4; ++c) {
            CardKnowledge &knol = handKnowledge_[player][c];
            if (!knol.isPlayable) continue;
            if (knol.mustBe(Value(5))) continue;
            if (knol.mustBe(played_card.color) || knol.mustBe(played_card.value)) {
                /* This card might or might not be playable, anymore. */
                knol.isPlayable = false;
            }
        }
    }
}

void SimpleBot::pleaseMakeMove(Server &server)
{
    assert(server.whoAmI() == me_);
    assert(server.activePlayer() == me_);

    /* If I have a playable card, play it.
     * Otherwise, if someone else has an unknown-playable card, hint it.
     * Otherwise, just discard my oldest (index-0) card. */

    for (int i=0; i < 4; ++i) {
        if (handKnowledge_[me_][i].isPlayable) {
            this->prepareToPostObserve(server);
            this->invalidateKnol(me_, i);
            server.pleasePlay(i);
            return;
        }
    }
    
    if (server.mulligansRemaining() > 0) {
        const int numPlayers = handKnowledge_.size();
        int best_so_far = 0;
        int player_to_hint = -1;
        int color_to_hint = -1;
        int value_to_hint = -1;
        for (int i = 1; i < numPlayers; ++i) {
            const int partner = (me_ + i) % numPlayers;
            assert(partner != me_);
            const std::vector<Card> partners_hand = server.handOfPlayer(partner);
            bool is_really_playable[4];
            for (int c=0; c < 4; ++c) {
                is_really_playable[c] =
                    server.pileOf(partners_hand[c].color).nextValueIs(partners_hand[c].value);
            }
            /* Can we construct a color hint that gives our partner information
             * about unknown-playable cards, without also including any
             * unplayable cards? */
            for (Color color = RED; color <= BLUE; ++color) {
                int information_content = 0;
                bool misinformative = false;
                for (int c=0; c < 4; ++c) {
                    if (partners_hand[c].color != color) continue;
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
                if (information_content > best_so_far) {
                    best_so_far = information_content;
                    color_to_hint = color;
                    value_to_hint = -1;
                    player_to_hint = partner;
                }
            }

            for (int value = 1; value <= 5; ++value) {
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
                if (information_content > best_so_far) {
                    best_so_far = information_content;
                    color_to_hint = -1;
                    value_to_hint = value;
                    player_to_hint = partner;
                }
            }
        }

        if (best_so_far != 0) {
            /* Update our knowledge, since we won't get a callback from the server. */
            const std::vector<Card> partners_hand = server.handOfPlayer(player_to_hint);
            std::vector<CardKnowledge> &knol = handKnowledge_[player_to_hint];
            for (int c=0; c < 4; ++c) {
                if (color_to_hint != -1 && partners_hand[c].color == color_to_hint) {
                    knol[c].setMustBe(Color(color_to_hint));
                    knol[c].isPlayable = true;
                } else if (value_to_hint != -1 && partners_hand[c].value == value_to_hint) {
                    knol[c].setMustBe(Value(value_to_hint));
                    knol[c].isPlayable = true;
                }
            }

            /* Give the hint. */
            if (color_to_hint != -1) {
                server.pleaseGiveColorHint(player_to_hint, Color(color_to_hint));
            } else if (value_to_hint != -1) {
                server.pleaseGiveValueHint(player_to_hint, Value(value_to_hint));
            } else {
                assert(false);
            }

            return;
        }
    }

    /* We couldn't find a good hint to give, or else we're out of mulligans.
     * Discard a card. */
    server.pleaseDiscard(0);
    this->invalidateKnol(me_, 0);
}

