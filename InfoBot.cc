
#include "Hanabi.h"
#include "InfoBot.h"

#include <cassert>
#include <map>
#include <memory>
#include <set>
#include <vector>

using Hanabi::Card;
using Hanabi::CardIndices;
using Hanabi::Color;
using Hanabi::Value;
using Hanabi::RED;
using Hanabi::BLUE;

struct CardLessThan {
    CardLessThan() = default;
    bool operator()(Card a, Card b) const noexcept {
        if ((int)a.color != (int)b.color) return (int)a.color < (int)b.color;
        if ((int)a.value != (int)b.value) return (int)a.value < (int)b.value;
        return false;
    }
};

struct Hinted {
    enum Kind { COLOR, VALUE };
    explicit Hinted(Kind kind, int k) : kind(kind), color(Color(-1)), value(Value(-1)) {
        if (kind == Hinted::COLOR) color = Color(k);
        if (kind == Hinted::VALUE) value = Value(k);
    }
    static Hinted WithColor(Color k) { return Hinted(COLOR, int(k)); }
    static Hinted WithValue(Value v) { return Hinted(VALUE, int(v)); }
    Kind kind;
    Color color;
    Value value;

    friend bool operator<(const Hinted& a, const Hinted& b) noexcept {
        if (a.kind != b.kind) return (a.kind < b.kind);
        if (a.color != b.color) return (a.color < b.color);
        if (a.value != b.value) return (a.value < b.value);
        return false;
    }
};

struct Hint : public Hinted {
    int to;
    explicit Hint(Kind kind, int to, int k) : Hinted(kind, k), to(to) {}
};

class CardCounts {
    int8_t counts_[Hanabi::NUMCOLORS][5+1] {};
public:
    explicit CardCounts() = default;
    void increment(Card card) {
        counts_[card.color][card.value] += 1;
    }
    int remaining(Card card) const {
        return card.count() - counts_[card.color][card.value];
    }
};

class GameView {
    CardCounts discard;
    int fireworks[Hanabi::NUMCOLORS];
public:
    int deck_size;
    int total_cards;
    int num_players;
    int hand_size;
    int discard_size;
    int hints_remaining;
    int lives_remaining;
    int player; // whose turn is it?
private:
    int hints_total;
    int lives_total;
public:
    explicit GameView(const Hanabi::Server& server) {
        deck_size = server.cardsRemainingInDeck();
        total_cards = Hanabi::NUMCOLORS * 10;
        discard_size = server.discards().size();
        for (Card card : server.discards()) {
            discard.increment(card);
        }
        for (Color k = RED; k <= BLUE; ++k) {
            fireworks[k] = server.pileOf(Color(k)).size();
        }
        num_players = server.numPlayers();
        player = server.activePlayer();
        hand_size = server.handSize();
        hints_total = Hanabi::NUMHINTS;
        hints_remaining = server.hintStonesRemaining();
        lives_total = Hanabi::NUMMULLIGANS;
        lives_remaining = server.mulligansRemaining();
    }

    explicit GameView(int numPlayers, int handSize) {
        deck_size = Hanabi::NUMCOLORS * 10;
        total_cards = Hanabi::NUMCOLORS * 10;
        for (Color k = RED; k <= BLUE; ++k) {
            fireworks[k] = 0;
        }
        num_players = numPlayers;
        player = 0;
        hand_size = handSize;
        hints_total = Hanabi::NUMHINTS;
        hints_remaining = Hanabi::NUMHINTS;
        lives_total = Hanabi::NUMMULLIGANS;
        lives_remaining = Hanabi::NUMMULLIGANS;
    }

    // returns whether a card would place on a firework
    bool is_playable(Card card) const {
        return card.value == this->fireworks[card.color] + 1;
    }

    // best possible value we can get for firework of that color,
    // based on looking at discard + fireworks
    Value highest_attainable(Color color) const {
        const auto& firework = this->fireworks[color];
        int needed = firework + 1;
        for (int v = needed; v <= 5; ++v) {
            Card needed_card(color, v);
            if (this->discard.remaining(needed_card) == 0) {
                // already discarded all of these
                return Value(v - 1);
            }
        }
        return Value(5);
    }

    // is never going to play, based on discard + fireworks
    bool is_dead(Card card) const {
        const auto& firework = this->fireworks[card.color];
        if (card.value <= firework) {
            return true;
        } else {
            return (card.value > this->highest_attainable(card.color));
        }
    }

    // can be discarded without necessarily sacrificing score, based on discard + fireworks
    bool is_dispensable(Card card) const {
        const auto& firework = this->fireworks[card.color];
        int needed = firework + 1;
        if (card.value < needed) {
            return true;
        } else if (card.value > this->highest_attainable(card.color)) {
            return true;
        } else {
            return this->discard.remaining(card) != 1;
        }
    }

    std::vector<int> get_players() const {
        std::vector<int> result;
        for (int i=0; i < num_players; ++i) result.push_back(i);
        return result;
    }
};

class OwnedGameView : public GameView {
    const Hanabi::Server *server;
public:
    explicit OwnedGameView(const GameView& view, const Hanabi::Server& server) : GameView(view), server(&server) {}

    std::vector<Card> get_hand(int p) const {
        return server->handOfPlayer(p);
    }

    bool has_card(int player, Card card) const {
        for (Card c : server->handOfPlayer(player)) {
            if (c == card) return true;
        }
        return false;
    }

    bool can_see(Card card) const {
        int me = server->whoAmI();
        for (int p=0; p < num_players; ++p) {
            if (p != me && this->has_card(p, card)) return true;
        }
        return false;
    }

    bool someone_else_can_play() const {
        int me = server->whoAmI();
        for (int p=0; p < server->numPlayers(); ++p) {
            if (p == me) continue;
            for (auto&& card : server->handOfPlayer(p)) {
                if (this->is_playable(card)) {
                    return true;
                }
            }
        }
        return false;
    }
};

class CardPossibilityTable {
    int8_t counts_[Hanabi::NUMCOLORS][5+1] {};
public:
    explicit CardPossibilityTable() {
        for (Color k = RED; k <= BLUE; ++k) {
            for (int v = 1; v <= 5; ++v) {
                counts_[k][v] = Hanabi::Card(k, v).count();
            }
        }
    }
    static CardPossibilityTable from(const CardCounts& counts) {
        CardPossibilityTable result;
        for (Color k = RED; k <= BLUE; ++k) {
            for (int v = 1; v <= 5; ++v) {
                result.counts_[k][v] = counts.remaining(Hanabi::Card(k, v));
            }
        }
        return result;
    }
    void mark_false(Card card) {
        counts_[card.color][card.value] = 0;
    }
    void mark_color(Color color, bool yes) {
        for (Color k = RED; k <= BLUE; ++k) {
            for (int v = 1; v <= 5; ++v) {
                if ((k == color) != yes) {
                    mark_false(Card(k, v));
                }
            }
        }
    }
    void mark_value(Value value, bool yes) {
        for (Color k = RED; k <= BLUE; ++k) {
            for (int v = 1; v <= 5; ++v) {
                if ((v == value) != yes) {
                    mark_false(Card(k, v));
                }
            }
        }
    }
    bool is_possible(Card card) const {
        return (counts_[card.color][card.value] != 0);
    }
    bool can_be_color(Color color) const {
        for (int v = 1; v <= 5; ++v) {
            if (counts_[color][v] != 0) return true;
        }
        return false;
    }
    bool can_be_value(Value value) const {
        for (Color k = RED; k <= BLUE; ++k) {
            if (counts_[k][value] != 0) return true;
        }
        return false;
    }
    void decrement_weight_if_possible(Card card) {
        auto& count = counts_[card.color][card.value];
        if (count) count -= 1;
    }
    template<class F>
    double weighted_score(F score_fn) const {
        double total_score = 0;
        double total_weight = 0;
        for (Color k = RED; k <= BLUE; ++k) {
            for (int v = 1; v <= 5; ++v) {
                if (counts_[k][v] != 0) {
                    double weight = counts_[k][v];
                    double score = score_fn(Card(k, v));
                    total_weight += weight;
                    total_score += weight * score;
                }
            }
        }
        return total_score / total_weight;
    }
    double average_value() const {
        return this->weighted_score([](Card card) { return card.value; });
    }
    double total_weight() const {
        return this->weighted_score([](const Card&) { return 1.0; });
    }
    template<class F>
    double probability_of_predicate(F predicate) const {
        return this->weighted_score([&](Card card) { return predicate(card) ? 1.0 : 0.0; });
    }
    double probability_is_dead(const GameView& view) const {
        return this->probability_of_predicate([&](Card card) { return view.is_dead(card); });
    }
    double probability_is_playable(const GameView& view) const {
        return this->probability_of_predicate([&](Card card) { return view.is_playable(card); });
    }
    double probability_is_dispensable(const GameView& view) const {
        return this->probability_of_predicate([&](Card card) { return view.is_dispensable(card); });
    }
    std::vector<Card> get_possibilities() const {
        std::vector<Card> result;
        for (Color k = RED; k <= BLUE; ++k) {
            for (int v = 1; v <= 5; ++v) {
                if (counts_[k][v] != 0) {
                    result.push_back(Card(k, v));
                }
            }
        }
        return result;
    }
    bool is_determined() const {
        return get_possibilities().size() == 1;
    }
    bool color_determined() const {
        bool found = false;
        for (Color k = RED; k <= BLUE; ++k) {
            for (int v = 1; v <= 5; ++v) {
                if (counts_[k][v] != 0) {
                    if (found) return false;
                    found = true;
                    break;
                }
            }
        }
        assert(found);  // otherwise we didn't find any possibilities at all
        return false;
    }
    bool value_determined() const {
        bool found = false;
        for (int v = 1; v <= 5; ++v) {
            for (Color k = RED; k <= BLUE; ++k) {
                if (counts_[k][v] != 0) {
                    if (found) return false;
                    found = true;
                    break;
                }
            }
        }
        assert(found);  // otherwise we didn't find any possibilities at all
        return false;
    }
};

struct ModulusInformation {
    int modulus;
    int value;

    explicit ModulusInformation(int m, int v) : modulus(m), value(v) {
        assert(value < modulus);
    }

    static ModulusInformation none() {
        return ModulusInformation(1, 0);
    }

    void combine(ModulusInformation other) {
        value = value * other.modulus + other.value;
        modulus *= other.modulus;
    }

    ModulusInformation split(int m) {
        assert(modulus >= m);
        assert(modulus % m == 0);
        int original_modulus = modulus;
        int original_value = value;
        modulus /= m;
        int v = value / modulus;
        value -= (v * modulus);
        assert(original_modulus == m * modulus);
        assert(original_value == v * modulus + value);
        return ModulusInformation(m, v);
    }

    void cast_up(int m) {
        assert(modulus <= m);
        assert(value < m);
        modulus = m;
    }

    void cast_down(int m) {
        assert(modulus >= m);
        assert(value < m);
        modulus = m;
    }

    void add(ModulusInformation other) {
        assert(modulus == other.modulus);
        value = (value + other.value) % modulus;
    }

    void subtract(ModulusInformation other) {
        assert(modulus == other.modulus);
        value = (modulus + value - other.value) % modulus;
    }
};

class Question {
public:
    // how much info does this question ask for?
    virtual int info_amount() const = 0;
    // get the answer to this question, given cards
    virtual int answer(const std::vector<Card>&, const GameView&) const = 0;
    // process the answer to this question, updating card info
    virtual void acknowledge_answer(int answer, std::vector<CardPossibilityTable>&, const GameView&) const = 0;
    virtual ~Question() = default;

    ModulusInformation answer_info(const std::vector<Card>& hand, const GameView& view) const {
        return ModulusInformation(
            this->info_amount(),
            this->answer(hand, view)
        );
    }

    void acknowledge_answer_info(ModulusInformation answer, std::vector<CardPossibilityTable>& hand_info, const GameView& view) const {
        assert(this->info_amount() == answer.modulus);
        this->acknowledge_answer(answer.value, hand_info, view);
    }
};

class IsPlayable : public Question {
    int index;

    int info_amount() const override { return 2; }
    int answer(const std::vector<Card>& hand, const GameView& view) const override {
        const Card& card = hand[this->index];
        if (view.is_playable(card)) {
            return 1;
        } else {
            return 0;
        }
    }
    void acknowledge_answer(int answer, std::vector<CardPossibilityTable>& hand_info, const GameView& view) const override {
        auto& card_table = hand_info[this->index];
        auto possible = card_table.get_possibilities();
        for (auto&& card : possible) {
            if (view.is_playable(card)) {
                if (answer == 0) { card_table.mark_false(card); }
            } else {
                if (answer == 1) { card_table.mark_false(card); }
            }
        }
    }
public:
    explicit IsPlayable(int i) : index(i) {}
};

class CardPossibilityPartition : public Question {
    int index;
    int n_partitions;
    std::map<Card, int, CardLessThan> partition;

public:
    explicit CardPossibilityPartition(
        int index, int max_n_partitions, const CardPossibilityTable& card_table, const GameView& view)
    {
        int cur_block = 0;
        std::map<Card, int, CardLessThan> partition;
        int n_partitions = 0;

        bool has_dead = card_table.probability_is_dead(view) != 0.0;

        // TODO: group things of different colors and values?
        int effective_max = max_n_partitions;
        if (has_dead) {
            effective_max -= 1;
        }

        for (auto&& card : card_table.get_possibilities()) {
            if (!view.is_dead(card)) {
                partition.emplace(card, cur_block);
                cur_block = (cur_block + 1) % effective_max;
                if (n_partitions < effective_max) {
                    n_partitions += 1;
                }
            }
        }

        if (has_dead) {
            for (auto&& card : card_table.get_possibilities()) {
                if (view.is_dead(card)) {
                    partition.emplace(card, n_partitions);
                }
            }
            n_partitions += 1;
        }

        this->index = index;
        this->n_partitions = n_partitions;
        this->partition = std::move(partition);
    }

private:
    int info_amount() const override { return this->n_partitions; }
    int answer(const std::vector<Card>& hand, const GameView&) const override {
        const auto& card = hand[this->index];
        return this->partition.at(card);
    }
    void acknowledge_answer(int answer, std::vector<CardPossibilityTable>& hand_info, const GameView&) const override {
        auto& card_table = hand_info[this->index];
        std::vector<Card> possible = card_table.get_possibilities();
        for (const Card& card : possible) {
            if (this->partition.at(card) != answer) {
                card_table.mark_false(card);
            }
        }
    }
};

struct AugmentedCardPossibilities {
    CardPossibilityTable card_table;
    int i;
    double p_play;
    double p_dead;
    bool is_determined;

    explicit AugmentedCardPossibilities(CardPossibilityTable ct, int i, double p, double d, bool det) :
        card_table(std::move(ct)), i(i), p_play(p), p_dead(d), is_determined(det) {}
};

class InfoBotImpl : public InfoBot {
    int me;
    int numPlayers;
    std::vector<std::vector<CardPossibilityTable>> public_info;
    CardCounts public_counts; // what any newly drawn card could be
    GameView last_view; // the view on the previous turn

public:
    InfoBotImpl(int index, int numPlayers, int handSize) :
        me(index), numPlayers(numPlayers), last_view(numPlayers, handSize)
    {
        for (int i=0; i < numPlayers; ++i) {
            this->public_info.emplace_back(std::vector<CardPossibilityTable>(handSize));
        }
    }

    std::vector<std::shared_ptr<Question>> get_questions(int total_info, const GameView& view, const std::vector<CardPossibilityTable>& hand_info) const {
        std::vector<std::shared_ptr<Question>> questions;
        int info_remaining = total_info;
        auto add_question = [&](std::shared_ptr<Question> question) {
            info_remaining /= question->info_amount();
            questions.push_back(std::move(question));
            return info_remaining <= 1;
        };

        std::vector<AugmentedCardPossibilities> augmented_hand_info;
        for (int i=0; i < (int)hand_info.size(); ++i) {
            const auto& card_table = hand_info[i];
            double p_play = card_table.probability_is_playable(view);
            double p_dead = card_table.probability_is_dead(view);
            bool is_determined = card_table.is_determined();
            augmented_hand_info.emplace_back(
                card_table, i, p_play, p_dead, is_determined
            );
        }

        int known_playable = 0;
        for (auto&& knol : augmented_hand_info) {
            known_playable += (knol.p_play == 1.0);
        }

        if (known_playable == 0) {
            std::vector<AugmentedCardPossibilities> ask_play;
            for (auto&& knol : augmented_hand_info) {
                if (knol.is_determined) continue;
                if (knol.p_dead == 1.0) continue;
                if (knol.p_play == 1.0 || knol.p_play < 0.2) continue;
                ask_play.push_back(knol);
            }
            // sort by probability of play, then by index
            std::sort(ask_play.begin(), ask_play.end(), [](auto&& a, auto&& b) {
                // *higher* probabilities are better
                if (a.p_play != b.p_play) return (a.p_play > b.p_play);
                return (a.i < b.i);
            });

            for (const auto& knol : ask_play) {
                auto q = std::make_shared<IsPlayable>(knol.i);
                if (add_question(q)) {
                    return questions;
                }
            }
        }

        std::vector<AugmentedCardPossibilities> ask_partition;
        for (auto&& knol : augmented_hand_info) {
            if (knol.is_determined) continue;
            // TODO: possibly still valuable to ask?
            if (knol.p_dead == 1.0) continue;
            ask_partition.push_back(knol);
        }

        // sort by probability of play, then by index
        std::sort(ask_partition.begin(), ask_partition.end(), [](auto&& a, auto&& b) {
            // *higher* probabilities are better
            if (a.p_play != b.p_play) return (a.p_play > b.p_play);
            return (a.i < b.i);
        });

        for (const auto& knol : ask_partition) {
            auto q = std::make_shared<CardPossibilityPartition>(knol.i, info_remaining, knol.card_table, view);
            if (add_question(q)) {
                return questions;
            }
        }

        return questions;
    }

    ModulusInformation answer_questions(
        const std::vector<std::shared_ptr<Question>>& questions,
        const std::vector<Card>& hand, const GameView& view) const
    {
        ModulusInformation answer_info = ModulusInformation::none();
        for (auto&& question : questions) {
            auto new_answer_info = question->answer_info(hand, view);
            answer_info.combine(new_answer_info);
        }
        return answer_info;
    }

    ModulusInformation get_hint_info_for_player(int player, int total_info, const OwnedGameView& view) const {
        assert(player != me);
        const auto& hand_info = get_player_public_info(player);
        auto questions = this->get_questions(total_info, view, hand_info);
        auto answer = this->answer_questions(questions, view.get_hand(player), view);
        answer.cast_up(total_info);
        return answer;
    }

    ModulusInformation get_hint_sum_info(int total_info, const OwnedGameView& view) const {
        ModulusInformation sum_info(total_info, 0);
        for (int player : view.get_players()) {
            if (player == me) continue;
            auto answer = this->get_hint_info_for_player(player, total_info, view);
            sum_info.add(answer);
        }
        return sum_info;
    }

    void infer_own_from_hint_sum(ModulusInformation hint) {
        auto questions = this->get_questions(hint.modulus, this->last_view, this->get_my_public_info());
        int product = 1;
        for (auto&& b : questions) product *= b->info_amount();
        hint.cast_down(product);
        auto& hand_info = this->get_player_public_info_mut(me);
        {
            auto view = this->last_view;
            for (const auto& question : questions) {
                auto answer_info = hint.split(question->info_amount());
                question->acknowledge_answer_info(answer_info, hand_info, view);
            }
        }
    }

    void update_from_hint_sum(ModulusInformation hint, const OwnedGameView& view) {
        int hinter = view.player;
        for (int player : view.get_players()) {
            if (player != hinter && player != this->me) {
                if (true) {
                    auto hint_info = this->get_hint_info_for_player(player, hint.modulus, view);
                    hint.subtract(hint_info);
                }

                auto& hand_info = this->get_player_public_info_mut(player);
                auto hand = view.get_hand(player);
                auto questions = this->get_questions(hint.modulus, view, hand_info);
                for (auto&& question : questions) {
                    auto answer = question->answer(hand, view);
                    question->acknowledge_answer(answer, hand_info, view);
                }
            }
        }
        if (this->me == hinter) {
            assert(hint.value == 0);
        } else {
            this->infer_own_from_hint_sum(hint);
        }
    }

    // how badly do we need to play a particular card
    double get_average_play_score(const OwnedGameView& view, const CardPossibilityTable& card_table) {
        return card_table.weighted_score([&](const Card& card) {
            return this->get_play_score(view, card);
        });
    }

    double get_play_score(const OwnedGameView& view, const Card& card) {
        int num_with = 1;
        if (view.deck_size > 0) {
            for (int player : view.get_players()) {
                if (player != this->me) {
                    if (view.has_card(player, card)) {
                        num_with += 1;
                    }
                }
            }
        }
        return (10.0 - int(card.value)) / num_with;
    }

    std::vector<int> find_useless_cards(const GameView& view, const std::vector<CardPossibilityTable>& hand) {
        std::set<int> useless;
        std::map<Card, int, CardLessThan> seen;

        for (int i=0; i < (int)hand.size(); ++i) {
            const auto& card_table = hand[i];
            if (card_table.probability_is_dead(view) == 1.0) {
                useless.insert(i);
            } else {
                auto poss = card_table.get_possibilities();
                if (poss.size() == 1) {
                    Card card = poss[0];
                    if (seen.count(card)) {
                        // found a duplicate card
                        useless.insert(i);
                        useless.insert(seen.at(card));
                    } else {
                        seen.emplace(card, i);
                    }
                }
            }
        }
        std::vector<int> useless_vec;
        for (int i : useless) {
            useless_vec.push_back(i);
        }
        std::sort(useless_vec.begin(), useless_vec.end());
        return useless_vec;
    }

    const std::vector<CardPossibilityTable>& get_my_public_info() const {
        return this->public_info.at(this->me);
    }

    const std::vector<CardPossibilityTable>& get_player_public_info(int player) const {
        return this->public_info.at(player);
    }

    std::vector<CardPossibilityTable>& get_player_public_info_mut(int player) {
        return this->public_info.at(player);
    }

    void update_public_info_for_hint(const Hint& hint, CardIndices card_indices) {
        std::vector<CardPossibilityTable>& info = this->public_info.at(hint.to);

        // info.update_for_hint(hint, matches);
        if (hint.kind == Hinted::COLOR) {
            for (int i = 0; i < info.size(); ++i) {
                info[i].mark_color(hint.color, card_indices.contains(i));
            }
        } else if (hint.kind == Hinted::VALUE) {
            for (int i = 0; i < info.size(); ++i) {
                info[i].mark_value(hint.value, card_indices.contains(i));
            }
        } else {
            assert(false);
        }
    }

    void update_public_info_for_discard_or_play(GameView& view, int player, int index, Card card) {
        {
            std::vector<CardPossibilityTable>& info = this->public_info.at(player);
            assert(info[index].is_possible(card));
            info.erase(info.begin() + index);

            // push *before* incrementing public counts
            if (view.deck_size != 0) {
                info.push_back(CardPossibilityTable::from(this->public_counts));
            }
        }

        // TODO: decrement weight counts for fully determined cards, ahead of time

        // note: other_player could be player, as well
        // in particular, we will decrement the newly drawn card
        for (int other_player : view.get_players()) {
            auto& info = this->public_info.at(other_player);
            for (auto& card_table : info) {
                card_table.decrement_weight_if_possible(card);
            }
        }

        this->public_counts.increment(card);
    }

    std::vector<CardPossibilityTable> get_private_info(const Hanabi::Server& server) const {
        std::vector<CardPossibilityTable> info = this->get_my_public_info();
        for (auto& card_table : info) {
            for (int p=0; p < server.numPlayers(); ++p) {
                if (p == me) continue;
                for (Card card : server.handOfPlayer(p)) {
                    card_table.decrement_weight_if_possible(card);
                }
            }
        }
        return info;
    }

    int get_hint_index_score(const CardPossibilityTable& card_table, const GameView& view) const {
        if (card_table.probability_is_dead(view) == 1.0) {
            return 0;
        }
        if (card_table.is_determined()) {
            return 0;
        }
        // Do something more intelligent?
        int score = 1;
        if (!card_table.color_determined()) {
            score += 1;
        }
        if (!card_table.value_determined()) {
            score += 1;
        }
        return score;
    }

    int get_index_for_hint(const std::vector<CardPossibilityTable>& info, const GameView& view) const {
        std::vector<std::pair<int, int>> scores;
        for (int i=0; i < (int)info.size(); ++i) {
            auto&& card_table = info[i];
            int score = this->get_hint_index_score(card_table, view);
            scores.emplace_back(-score, i);
        }
        std::sort(scores.begin(), scores.end());
        return scores[0].second;
    }

    // how good is it to give this hint to this player?
    double hint_goodness(const Hanabi::Server& server, const Hinted& hinted, int hint_player, const GameView& view) const {
        auto hand = server.handOfPlayer(hint_player);

        // get post-hint hand_info
        auto hand_info = this->get_player_public_info(hint_player);
        int total_info  = 3 * (view.num_players - 1);
        auto questions = this->get_questions(total_info, view, hand_info);
        for (auto&& question : questions) {
            auto answer = question->answer(hand, view);
            question->acknowledge_answer(answer, hand_info, view);
        }

        double goodness = 1.0;
        for (int i=0; i < (int)hand_info.size(); ++i) {
            auto& card_table = hand_info[i];
            auto card = hand[i];
            if (card_table.probability_is_dead(view) == 1.0) {
                continue;
            }
            if (card_table.is_determined()) {
                continue;
            }
            auto old_weight = card_table.total_weight();
            if (hinted.kind == Hinted::COLOR) {
                card_table.mark_color(hinted.color, hinted.color == card.color);
            } else if (hinted.kind == Hinted::VALUE) {
                card_table.mark_value(hinted.value, hinted.value == card.value);
            } else {
                assert(false);
            }
            auto new_weight = card_table.total_weight();
            assert(new_weight <= old_weight);
            double bonus = (
                card_table.is_determined() ? 2 :
                card_table.probability_is_dead(view) == 1.0 ? 2 :
                1
            );
            goodness *= bonus * (old_weight / new_weight);
        }
        return goodness;
    }

    // Returns the number of ways to hint the player.
    int get_info_per_player(int player) const {
        const auto& info = this->get_player_public_info(player);

        // Determine if both:
        //  - it is public that there are at least two colors
        //  - it is public that there are at least two numbers

        bool may_be_all_one_color = false;
        for (Color k = RED; k <= BLUE; ++k) {
            if (std::all_of(info.begin(), info.end(), [=](auto&& card) { return card.can_be_color(k); })) {
                may_be_all_one_color = true;
                break;
            }
        }

        bool may_be_all_one_number = false;
        for (int v = 1; v <= 5; ++v) {
            if (std::all_of(info.begin(), info.end(), [=](auto&& card) { return card.can_be_value(Value(v)); })) {
                may_be_all_one_number = true;
                break;
            }
        }

        return (!may_be_all_one_color && !may_be_all_one_number) ? 4 : 3;
    }

    std::vector<int> get_other_players_starting_after(int player) const {
        std::vector<int> result;
        for (int i=0; i < numPlayers; ++i) {
            result.push_back((player + 1 + i) % numPlayers);
        }
        return result;
    }

    Hinted get_best_hint_of_options(const Hanabi::Server& server, int hint_player, std::set<Hinted> hint_option_set) const {
        const auto& view = this->last_view;

        // using hint goodness barely helps
        std::vector<std::pair<double, Hinted>> hint_options;
        for (auto&& hinted : hint_option_set) {
            double goodness = this->hint_goodness(server, hinted, hint_player, view);
            hint_options.emplace_back(goodness, hinted);
        }

        std::sort(hint_options.begin(), hint_options.end(), [](const auto& h1, const auto& h2) {
            return h1.first > h2.first;
        });

        if (hint_options.empty()) {
            // NOTE: Technically possible, but never happens
        } else {
            if (hint_options.size() > 1) {
                // debug!("Choosing amongst hint options: {:?}", hint_options);
            }
        }

        return std::move(hint_options[0].second);
    }

    void get_hint(Hanabi::Server& server) const {
        OwnedGameView view(this->last_view, server);

        // Can give up to 3(n-1) hints
        // For any given player with at least 4 cards, and index i, there are at least 3 hints that can be given.
        // 0. a value hint on card i
        // 1. a color hint on card i
        // 2. any hint not involving card i
        // However, if it is public info that the player has at least two colors
        // and at least two numbers, then instead we do
        // 2. any color hint not involving i
        // 3. any color hint not involving i

        // TODO: make it so space of hints is larger when there is
        // knowledge about the cards?

        std::vector<int> info_per_player;
        int total_info = 0;
        for (int player : this->get_other_players_starting_after(this->me)) {
            int x = this->get_info_per_player(player);
            info_per_player.push_back(x);
            total_info += x;
        }
        auto hint_info = this->get_hint_sum_info(total_info, view);

        int hint_type = hint_info.value;
        int player_amt = 0;
        while (hint_type >= info_per_player[player_amt]) {
            hint_type -= info_per_player[player_amt];
            player_amt += 1;
        }
        int hint_info_we_can_give_to_this_player = info_per_player[player_amt];

        int hint_player = (this->me + 1 + player_amt) % numPlayers;

        const auto& hand = view.get_hand(hint_player);
        int card_index = this->get_index_for_hint(this->get_player_public_info(hint_player), view);
        const auto& hint_card = hand[card_index];

        Hinted hinted = (hint_info_we_can_give_to_this_player == 3) ? (
            (hint_type == 0) ? Hinted::WithValue(hint_card.value) :
            (hint_type == 1) ? Hinted::WithColor(hint_card.color) :
            (hint_type == 2) ? [&]() -> Hinted {
                // NOTE: this doesn't do that much better than just hinting
                // the first thing that doesn't match the hint_card
                std::set<Hinted> hint_option_set;
                for (auto&& card : hand) {
                    if (card.color != hint_card.color) hint_option_set.insert(Hinted::WithColor(card.color));
                    if (card.value != hint_card.value) hint_option_set.insert(Hinted::WithValue(card.value));
                }
                return this->get_best_hint_of_options(server, hint_player, hint_option_set);
            }() : []() -> Hinted {
                assert(!"Invalid hint type");
            }()
        ) : (
            (hint_type == 0) ? Hinted::WithValue(hint_card.value) :
            (hint_type == 1) ? Hinted::WithColor(hint_card.color) :
            (hint_type == 2) ? [&]() -> Hinted {
                // Any value hint for a card other than the first
                std::set<Hinted> hint_option_set;
                for (auto&& card : hand) {
                    if (card.value != hint_card.value) hint_option_set.insert(Hinted::WithValue(card.value));
                }
                return this->get_best_hint_of_options(server, hint_player, hint_option_set);
            }() :
            (hint_type == 2) ? [&]() -> Hinted {
                    // Any color hint for a card other than the first
                std::set<Hinted> hint_option_set;
                for (auto&& card : hand) {
                    if (card.color != hint_card.color) hint_option_set.insert(Hinted::WithColor(card.color));
                }
                return this->get_best_hint_of_options(server, hint_player, hint_option_set);
            }() : []() -> Hinted {
                assert(!"Invalid hint type");
            }()
        );

        if (hinted.kind == Hinted::COLOR) {
            return server.pleaseGiveColorHint(hint_player, hinted.color);
        } else {
            return server.pleaseGiveValueHint(hint_player, hinted.value);
        }
    }

    void infer_from_hint(const Hint& hint, int hinter, CardIndices result, const OwnedGameView& view) {
        std::vector<int> info_per_player;
        int total_info = 0;
        for (int player : this->get_other_players_starting_after(hinter)) {
            int x = this->get_info_per_player(player);
            info_per_player.push_back(x);
            total_info += x;
        }

        int n = numPlayers;

        int player_amt = (n + hint.to - hinter - 1) % n;

        int amt_from_prev_players = 0;
        for (int i=0; i < player_amt; ++i) {
            amt_from_prev_players += info_per_player[i];
        }
        int hint_info_we_can_give_to_this_player = info_per_player[player_amt];

        int card_index = this->get_index_for_hint(this->get_player_public_info(hint.to), this->last_view);
        int hint_type;
        if (hint_info_we_can_give_to_this_player == 3) {
            if (result[card_index]) {
                if (hint.kind == Hinted::VALUE) {
                    hint_type = 0;
                } else {
                    hint_type = 1;
                }
            } else {
                hint_type = 2;
            }
        } else {
            if (result[card_index]) {
                if (hint.kind == Hinted::VALUE) {
                    hint_type = 0;
                } else {
                    hint_type = 1;
                }
            } else {
                if (hint.kind == Hinted::VALUE) {
                    hint_type = 2;
                } else {
                    hint_type = 3;
                }
            }
        }
        int hint_value = amt_from_prev_players + hint_type;

        ModulusInformation mod_info(total_info, hint_value);

        this->update_from_hint_sum(mod_info, view);
    }

    void pleaseMakeMove(Hanabi::Server& server) override
    {
        // we already stored the view
        OwnedGameView view(this->last_view, server);

        auto private_info = this->get_private_info(server);

        std::vector<std::pair<int, CardPossibilityTable>> playable_cards;
        for (int i=0; i < private_info.size(); ++i) {
            const auto& card_table = private_info[i];
            if (card_table.probability_is_playable(view) == 1.0) {
                playable_cards.emplace_back(i, card_table);
            }
        }

        if (!playable_cards.empty()) {
            // play the best playable card
            // the higher the play_score, the better to play
            double play_score = -1.0;
            int play_index = 0;

            for (const auto& kv : playable_cards) {
                const auto& index = kv.first;
                const auto& card_table = kv.second;
                double score = this->get_average_play_score(view, card_table);
                if (score > play_score) {
                    play_score = score;
                    play_index = index;
                }
            }

            return server.pleasePlay(play_index);
        }

        int discard_threshold =
            view.total_cards
            - (Hanabi::NUMCOLORS * 5)
            - (view.num_players * view.hand_size);

        // make a possibly risky play
        // TODO: consider removing this, if we improve information transfer
        if (view.lives_remaining > 1 &&
            view.discard_size <= discard_threshold)
        {
            std::vector<std::tuple<int, CardPossibilityTable, double>> risky_playable_cards;
            for (int i=0; i < private_info.size(); ++i) {
                const auto& card_table = private_info[i];
                // card is either playable or dead
                if (card_table.probability_of_predicate([&](Card card) {
                    return view.is_playable(card) || view.is_dead(card);
                }) != 1.0) {
                    continue;
                }
                double p = card_table.probability_is_playable(view);
                risky_playable_cards.emplace_back(i, card_table, p);
            }

            if (!risky_playable_cards.empty()) {
                std::sort(risky_playable_cards.begin(), risky_playable_cards.end(), [&](const auto& a, const auto& b) {
                    return std::get<2>(a) > std::get<2>(b);
                });

                auto maybe_play = risky_playable_cards[0];
                if (std::get<2>(maybe_play) > 0.75) {
                    return server.pleasePlay(std::get<0>(maybe_play));
                }
            }
        }

        auto public_useless_indices = this->find_useless_cards(view, this->get_my_public_info());
        auto useless_indices = this->find_useless_cards(view, private_info);

        if (view.discard_size <= discard_threshold) {
            // if anything is totally useless, discard it
            if (public_useless_indices.size() > 1) {
                auto info = this->get_hint_sum_info(public_useless_indices.size(), view);
                return server.pleaseDiscard(public_useless_indices[info.value]);
            } else if (!useless_indices.empty()) {
                // TODO: have opponents infer that i knew a card was useless
                // TODO: after that, potentially prefer useless indices that arent public
                return server.pleaseDiscard(useless_indices[0]);
            }
        }

        // hinting is better than discarding dead cards
        // (probably because it stalls the deck-drawing).
        if (view.hints_remaining > 0) {
            if (view.someone_else_can_play()) {
                return this->get_hint(server);
            }
        }

        // if anything is totally useless, discard it
        if (public_useless_indices.size() > 1) {
            auto info = this->get_hint_sum_info(public_useless_indices.size(), view);
            return server.pleaseDiscard(public_useless_indices[info.value]);
        } else if (!useless_indices.empty()) {
            return server.pleaseDiscard(useless_indices[0]);
        }

        // NOTE: the only conditions under which we would discard a potentially useful card:
        // - we have no known useless cards
        // - there are no hints remaining OR nobody else can play

        // Play the best discardable card
        double compval = 0.0;
        int index = 0;
        for (int i=0; i < (int)private_info.size(); ++i) {
            const auto& card_table = private_info[i];
            double probability_is_seen = card_table.probability_of_predicate([&](Card card) {
                return view.can_see(card);
            });
            double my_compval =
                20.0 * probability_is_seen
                + 10.0 * card_table.probability_is_dispensable(view)
                + card_table.average_value();

            if (my_compval > compval) {
                compval = my_compval;
                index = i;
            }
        }
        return server.pleaseDiscard(index);
    }

    void pleaseObserveColorHint(const Hanabi::Server& server, int from, int to, Color color, CardIndices card_indices) override
    {
        OwnedGameView view(this->last_view, server);
        Hint hint(Hinted::COLOR, to, int(color));
        this->infer_from_hint(hint, from, card_indices, view);
        this->update_public_info_for_hint(hint, card_indices);
    }

    void pleaseObserveValueHint(const Hanabi::Server& server, int from, int to, Value value, CardIndices card_indices) override
    {
        OwnedGameView view(this->last_view, server);
        Hint hint(Hinted::VALUE, to, int(value));
        this->infer_from_hint(hint, from, card_indices, view);
        this->update_public_info_for_hint(hint, card_indices);
    }

    void pleaseObserveBeforeDiscard(const Hanabi::Server& server, int from, int card_index) override
    {
        OwnedGameView view(this->last_view, server);
        auto known_useless_indices = this->find_useless_cards(
            this->last_view, this->get_player_public_info(from)
        );
        if (known_useless_indices.size() > 1) {
            // unwrap is safe because *if* a discard happened, and there were known
            // dead cards, it must be a dead card
            int value = -1;
            for (int i=0; i < (int)known_useless_indices.size(); ++i) {
                if (known_useless_indices[i] == card_index) {
                    value = i;
                    break;
                }
            }
            assert(value != -1);
            this->update_from_hint_sum(ModulusInformation(known_useless_indices.size(), value), view);
        }
        this->update_public_info_for_discard_or_play(this->last_view, from, card_index, server.activeCard());
    }

    void pleaseObserveBeforePlay(const Hanabi::Server& server, int from, int card_index) override
    {
        this->update_public_info_for_discard_or_play(this->last_view, from, card_index, server.activeCard());
    }

    void pleaseObserveBeforeMove(const Hanabi::Server& server) override
    {
        this->last_view = GameView(server);
    }
};

std::unique_ptr<InfoBot> InfoBot::makeImpl(int index, int numPlayers, int handSize)
{
    return std::make_unique<InfoBotImpl>(index, numPlayers, handSize);
}
