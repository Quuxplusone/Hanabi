
#include <cassert>
#include <climits>
#include <cstring>
#include <iostream>
#include "Hanabi.h"
#include "NewCheatBot.h"

using namespace Hanabi;

static int numPlayers;
static std::vector<std::vector<Card> > hands;
static std::vector<Card> discards;
static int finalCountdown;

/* Apple's llvm-gcc 4.2.1 does not support this #pragma,
 * and GCC in general does not support it with non-POD
 * types such as std::vector (Bugzilla bug 27557).
 * The appropriate workaround in both cases is to build
 * run_CheatBot without OpenMP enabled. */
#pragma omp threadprivate(numPlayers,hands,discards)

template<typename T, int N = 5>
class smallvector
{
    int size_;
    T data_[N];

    typedef T *Iterator;
public:
    smallvector() : size_(0) {}
    T& operator[] (int i) { return data_[i]; }
    const T& operator[] (int i) const { return data_[i]; }
    size_t size() const { return size_; }
    size_t capacity() const { return N; }
    void reserve(size_t cap) { assert(cap <= N); (void)cap; }
    void resize(size_t len) { assert(len <= N); size_ = len; }
    void push_back(const T& t) { assert(size_ < N); data_[size_++] = t; }
    Iterator begin() { return data_; }
    Iterator end() { return data_ + size_; }
    void erase(Iterator it) {
        Iterator endPtr = end();
        if (it != endPtr) {
            for (++it; it != endPtr; ++it) {
                *(it - 1) = *it;
            }
        }
        size_ -= 1;
    }
};

static Card computeLastCardInDeck()
{
    int count[Hanabi::NUMCOLORS][5+1];
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            count[k][v] = Card(k,v).count();
        }
    }
    for (int i=0; i < discards.size(); ++i) {
        const Card &card = discards[i];
        count[card.color][card.value] -= 1;
    }
    for (int p=0; p < hands.size(); ++p) {
        for (int i=0; i < hands[p].size(); ++i) {
            const Card &card = hands[p][i];
            count[card.color][card.value] -= 1;
        }
    }
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = 1; v <= 5; ++v) {
            if (count[k][v] != 0) {
                return Card(k,v);
            }
        }
    }
    assert(false);
    return Card(RED,-1);
}

NewCheatBot::NewCheatBot(int index, int n, int /*handSize*/)
{
    me_ = index;
    numPlayers = n;
    hands.resize(n);
    discards.clear();
    finalCountdown = 0;
}

void NewCheatBot::pleaseObserveBeforeMove(const Server &server)
{
    if (me_ == 0) {
        for (int p=1; p < hands.size(); ++p) {
            hands[p] = server.handOfPlayer(p);
        }
        if (server.cardsRemainingInDeck() == 0) {
            finalCountdown = (finalCountdown == 0) ? numPlayers : (finalCountdown - 1);
            assert(finalCountdown != 0);
        }
    } else if (me_ == 1) {
        hands[0] = server.handOfPlayer(0);
    }
}

void NewCheatBot::pleaseObserveBeforeDiscard(const Server &server, int, int)
{
    if (me_ == 0) {
        discards.push_back(server.activeCard());
    }
}

void NewCheatBot::pleaseObserveBeforePlay(const Server &, int, int) { }
void NewCheatBot::pleaseObserveColorHint(const Server &, int, int, Color, CardIndices) { }
void NewCheatBot::pleaseObserveValueHint(const Server &, int, int, Value, CardIndices) { }
void NewCheatBot::pleaseObserveAfterMove(const Server &) { }

enum Kind { DISCARD, PLAY, HINT, FAKE };

struct Move {
    Kind kind;
    int cardIndex;

    Move() {}
    Move(Kind k): kind(k), cardIndex(0) {}

    void execute(Server& server) const;
};

void Move::execute(Server& server) const
{
    switch (kind) {
        case DISCARD:
            server.pleaseDiscard(cardIndex);
            break;
        case PLAY:
            server.pleasePlay(cardIndex);
            break;
        case HINT: {
            int who = (server.whoAmI() + 1) % server.numPlayers();
            server.pleaseGiveColorHint(who, server.handOfPlayer(who)[0].color);
            break;
        }
        case FAKE:
            assert(false);
            break;
    }
}

struct State {
    int discards_[Hanabi::NUMCOLORS][6];
    smallvector<smallvector<const Card*> > hands_;
    int piles_[Hanabi::NUMCOLORS];
    int who_;  /* whose turn it is */
    int hintStones_;
    int mulligans_;
    int deck_;
    int score_;  /* the sum of piles_ */
    int finalCountdown_;  /* turns remaining in the game */
    static Card lastCard_;  /* if there's just one card left in the draw pile (i.e. deck = 1) */

    State(const Server &server);
    void apply(Move move);
    int valuation() const;
    bool gameIsDefinitelyOver() const;
    bool isValuable(const Card &card) const;
};

Card State::lastCard_(RED,1);  /* out-of-line definition for static member variable */

State::State(const Server &server)
{
    memset(discards_, '\0', sizeof discards_);
    std::vector<Card> discards = server.discards();
    for (int i=0; i < discards.size(); ++i) {
        Card card = discards[i];
        discards_[card.color][card.value] += 1;
    }
    hands_.resize(hands.size());
    for (int i=0; i < hands.size(); ++i) {
        hands_[i].resize(hands[i].size());
        for (int j=0; j < hands[i].size(); ++j) {
            hands_[i][j] = &hands[i][j];
        }
    }
    score_ = 0;
    for (Color k = RED; k <= BLUE; ++k) {
        piles_[k] = server.pileOf(k).size() + 1;
        score_ += piles_[k] - 1;
    }
    who_ = server.whoAmI();
    hintStones_ = server.hintStonesRemaining();
    mulligans_ = server.mulligansRemaining();
    deck_ = server.cardsRemainingInDeck();
    if (deck_ == 1) {
        State::lastCard_ = computeLastCardInDeck();
    }
    finalCountdown_ = finalCountdown;
}

void State::apply(Move move)
{
    switch (move.kind) {
        case DISCARD: {
            assert(hintStones_ < Hanabi::NUMHINTS);
            smallvector<const Card*> &hand = hands_[who_];
            assert(0 <= move.cardIndex && move.cardIndex < hand.size());
            if (hand[move.cardIndex] != NULL) {
                const Card &card = *hand[move.cardIndex];
                discards_[card.color][card.value] += 1;
            }
            hand.erase(hand.begin() + move.cardIndex);
            if (deck_ != 0) {
                hand.push_back(deck_ == 1 ? &State::lastCard_ : NULL);
                deck_ -= 1;
            }
            hintStones_ += 1;
            break;
        }
        case PLAY: {
            smallvector<const Card*> &hand = hands_[who_];
            assert(0 <= move.cardIndex && move.cardIndex < hand.size());
            const Card* card = hand[move.cardIndex];
            if (card != NULL && card->value == piles_[card->color]) {
                piles_[card->color] += 1;
                score_ += 1;
                if (card->value == 5 && hintStones_ < Hanabi::NUMHINTS) ++hintStones_;
            } else {
                mulligans_ -= 1;
            }
            hand.erase(hand.begin() + move.cardIndex);
            if (deck_ != 0) {
                hand.push_back(deck_ == 1 ? &State::lastCard_ : NULL);
                deck_ -= 1;
            }
            break;
        }
        case HINT: {
            assert(hintStones_ > 0);
            hintStones_ -= 1;
            break;
        }
        case FAKE: {
            assert(false);
            break;
        }
    }

    if (finalCountdown_ != 0) {
        assert(deck_ == 0);
        finalCountdown_ -= 1;
    } else if (deck_ == 0) {
        finalCountdown_ = numPlayers;
    }
    who_ = (who_ + 1) % hands_.size();
}

int State::valuation() const
{
    if (score_ == 25) return INT_MAX;
    if (mulligans_ == 0 || (deck_ == 0 && finalCountdown_ == 0)) {
        return 10*score_;
    }

    int score = score_ * 11;
    for (Color k = RED; k <= BLUE; ++k) {
        for (int v = piles_[k]; v <= 5; ++v) {
            if (discards_[k][v] == Card(k,v).count()) {
                const int lost_cards = (6 - v);
                score -= (10 * lost_cards);
            } else if (discards_[k][v]) {
                score -= 1;
            }
        }
    }
    score -= 10 * (3 - mulligans_);
    return score;
}

bool State::gameIsDefinitelyOver() const
{
    if (mulligans_ == 0) return true;
    if (score_ == 25) return true;
    if (deck_ == 0 && finalCountdown_ == 0) return true;
    return false;
}

bool State::isValuable(const Card &card) const
{
    return piles_[card.color] <= card.value && (discards_[card.color][card.value] == card.count() - 1);
}

void getAllPossibleMoves(const State &state, smallvector<Move,11> &moves)
{
    const smallvector<const Card*> &hand = state.hands_[state.who_];
    Move m;
    bool haveSeenUnknown = false;
    for (int ci = 0; ci < hand.size(); ++ci) {
        if (hand[ci] == NULL) {
            if (haveSeenUnknown) continue;
            haveSeenUnknown = true;
        }

        m.cardIndex = ci;
        if ((state.deck_ < 10) || (hand[ci] != NULL && hand[ci]->value == state.piles_[hand[ci]->color])) {
            /* don't use mulligans until late in the game, to reduce branching factor */
            m.kind = PLAY;
            moves.push_back(m);
        }
        if (state.hintStones_ < Hanabi::NUMHINTS &&
            (state.hintStones_ <= 2 || hand[ci] == NULL || state.isValuable(*hand[ci]))) {
            m.kind = DISCARD;
            moves.push_back(m);
        }
    }
    if (state.hintStones_ > 0) {
        m.kind = HINT;
        moves.push_back(m);
    }
}

std::pair<Move,int> getBestMoveFromState(const State& state, int recursionDepth)
{
    if (recursionDepth <= 0 || state.gameIsDefinitelyOver()) {
        return std::make_pair(Move(FAKE), state.valuation());
    }

    smallvector<Move,11> moves;
    moves.reserve(5+5+1);
    getAllPossibleMoves(state, moves);
    assert(moves.size() >= 1);
    const int newRecursionDepth = recursionDepth - 1;
    std::pair<Move, int> bestmv = std::make_pair(Move(HINT), INT_MIN);
    for (int i=0; i < moves.size(); ++i) {
        State newState = state;
        newState.apply(moves[i]);
        std::pair<Move, int> mv = getBestMoveFromState(newState, newRecursionDepth);
        if (mv.second > bestmv.second) {
            bestmv = std::make_pair(moves[i], mv.second);
        }
    }
    return bestmv;
}

void NewCheatBot::pleaseMakeMove(Server &server)
{
    assert(server.whoAmI() == me_);
    assert(server.activePlayer() == me_);

    const State initialState(server);
    assert(!initialState.gameIsDefinitelyOver());
    const int recursionDepth = 4;
    std::pair<Move,int> mv = getBestMoveFromState(initialState, recursionDepth);
    mv.first.execute(server);
}
