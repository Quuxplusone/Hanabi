
struct CardLessThan {
    CardLessThan() = default;
    bool operator()(Hanabi::Card a, Hanabi::Card b) const noexcept {
        if ((int)a.color != (int)b.color) return (int)a.color < (int)b.color;
        if ((int)a.value != (int)b.value) return (int)a.value < (int)b.value;
        return false;
    }
};


class ModulusInformation {
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

    ModulusInformation split(int m) const {
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
    virtual int info_amount() const;
    // get the answer to this question, given cards
    virtual int answer(const Cards&, const OwnedGameView&) const;
    // process the answer to this question, updating card info
    virtual void acknowledge_answer(int answer, HandInfo<CardPossibilityTable>&, const OwnedGameView&) const;

    ModulusInformation answer_to(const Cards& hand, const OwnedGameView& view) const {
        return ModulusInformation(
            this->info_amount(),
            this->answer(hand, view)
        );
    }

    void acknowledge_answer_info(ModulusInformation answer, HandInfo<CardPossibilityTable>& hand_info, const OwnedGameView& view) const {
        assert(this->info_amount() == answer.modulus);
        this->acknowledge_answer(answer.value, hand_info, view);
    }
};

struct IsPlayable : public Question {
    int index;

    int info_amount() const override { return 2; }
    int answer(const Cards& hand, const OwnedGameView& view) const override {
        const auto& card = hand[this->index];
        if (view.get_board().is_playable(card)) {
            return 1;
        } else {
            return 0;
        }
    }
    void acknowledge_answer(int answer, HandInfo<CardPossibilityTable>& hand_info, const OwnedGameView& view) const override {
        auto& card_table = hand_info[this->index];
        auto possible = card_table.get_possibilities();
        for (auto&& card : possible) {
            if (view.get_board().is_playable(card)) {
                if (answer == 0) { card_table.mark_false(card); }
            } else {
                if (answer == 1) { card_table.mark_false(card); }
            }
        }
    }
};

struct CardPossibilityPartition : public Question {
    int index;
    int n_partitions;
    std::map<Card, int, CardLessThan> partition;

    explicit CardPossibilityPartition(
        int index, int max_n_partitions, const CardPossibilityTable& card_table, const OwnedGameView& view)
    {
        int cur_block = 0;
        std::map<Card, int, CardLessThan> partition;
        int n_partitions = 0;

        bool has_dead = card_table.probability_is_dead(&view.board) != 0.0;

        // TODO: group things of different colors and values?
        int effective_max = max_n_partitions;
        if (has_dead) {
            effective_max -= 1;
        }

        for (auto&& card : card_table.get_possibilities()) {
            if (!view.board.is_dead(card)) {
                partition.insert(card, cur_block);
                cur_block = (cur_block + 1) % effective_max;
                if n_partitions < effective_max {
                    n_partitions += 1;
                }
            }
        }

        if (has_dead) {
            for (auto&& card : card_table.get_possibilities()) {
                if (view.board.is_dead(card)) {
                    partition.insert(card, n_partitions);
                }
            }
            n_partitions += 1;
        }

        this->index = index;
        this->n_partitions = n_partitions;
        this->partition = std::move(partition);
    }

    int info_amount() const override { return this->n_partitions; }
    int answer(const Cards& hand, const OwnedGameView&) const override {
        const auto& card = hand[this->index];
        return this->partition.at(card);
    }
    void acknowledge_answer(int answer, HandInfo<CardPossibilityTable>& hand_info, const OwnedGameView&) const override {
        auto& card_table = hand_info[this->index];
        int possible = card_table.get_possibilities();
        for (auto&& card : possible) {
            if (this->partition.at(card) != answer) {
                card_table.mark_false(card);
            }
        }
    }
};

struct AugmentedCardPossibilities {
    CardPossibilities card_table;
    int i;
    double p_play;
    double p_dead;
    bool is_determined;
};

struct InformationPlayerStrategy {
    int me;
    std::map<int, HandInfo<CardPossibilityTable>> public_info;
    CardCounts public_counts; // what any newly drawn card should be
    OwnedGameView last_view; // the view on the previous turn

    InformationPlayerStrategy(int player, const BorrowedGameView& view) :
        me(player), last_view(view)
    {
        for (int player : view.board.get_players()) {
            HandInfo hand_info(view.board.hand_size);
            this->public_info.emplace(player, hand_info);
        }
    }

    std::vector<std::shared_ptr<Question>> get_questions(int total_info, const OwnedGameView& view, const HandInfo<CardPossibilityTable>& hand_info) {
        std::vector<std::shared_ptr<Question>> questions;
        int info_remaining = total_info;
        auto add_question = [](std::shared_ptr<Question> question) {
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

        for (const auto& knol : ask_play) {
            auto q = std::make_shared<CardPossibilityPartition>(knol.i, info_remaining, knol.card_table, view);
            if (add_question(q)) {
                return questions;
            }
        }

        return questions;
    }

    ModulusInformation answer_questions(
        const std::vector<std::shared_ptr<Question>>& questions,
        const Cards& hand, const OwnedGameView& view)
    {
        ModulusInformation answer_info = ModulusInformation::none();
        for (auto&& question : questions) {
            auto new_answer_info = question->answer_info(hand, view);
            answer_info.combine(new_answer_info);
        }
        return answer_info;
    }

    ModulusInformation get_hint_info_for_player(int player, int total_info, const OwnedGameView& view) {
        assert(player != me);
        auto hand_info = get_player_public_info(player);
        auto questions = this->get_questions(total_info, view, hand_info);
        auto answer = this->answer_questions(questions, view.get_hand(player), view);
        answer.cast_up(total_info);
        return answer;
    }

    ModulusInformation get_hint_sum_info(int total_info, const OwnedGameView& view) {
        ModulusInformation sum_info(total_info, 0);
        for (int player : view.get_board().get_players()) {
            if (player == me) continue;
            auto answer = self.get_hint_info_for_player(player, total_info, view);
            sum_info.add(answer);
        }
        return sum_info;
    }

    void infer_own_from_hint_sum(ModulusInformation hint) {
        auto questions = this->get_questions(hint.modulus, this->last_view, this->get_my_public_info());
        int product = 1;
        for (auto&& b : questions) product *= b->info_amount();
        hint.cast_down(product);
        auto hand_info = this->take_public_info(me);
        {
            auto view = this->last_view;
            for (const auto& question : questions) {
                auto answer_info = hint.split(question->info_amount());
                question->acknowledge_answer_info(answer_info, hand_info, view);
            }
        }
        this->return_public_info(me, hand_info);
    }

    void update_from_hint_sum(ModulusInformation hint) {
        int hinter = this->last_view.board.player;
        auto players = this->last_view.board.get_players();
        for (int player : players) {
            if (player != hinter && player != this->me) {
                {
                    auto hint_info = this->get_hint_info_for_player(player, hint.modulus, this->last_view);
                    hint.subtract(hint_info);
                }

                auto hand_info = this->take_public_info(player);

                {
                    auto view = this->last_view;
                    auto hand = view.get_hand(player);
                    auto questions = this->get_questions(hint.modulus, view, hand_info);
                    for (auto&& question : questions) {
                        auto answer = question->answer(hand, view);
                        question->acknowledge_answer(answer, hand_info, view);
                    }
                }
                this->return_public_info(player, hand_info);
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
        if (view.board.deck_size > 0) {
            for (int player : view.board.get_players()) {
                if (player != this->me) {
                    if (view.has_card(player, card)) {
                        num_with += 1;
                    }
                }
            }
        }
        return (10.0 - int(card.value)) / num_with;
    }

    std::vector<int> find_useless_cards(const OwnedGameView& view, const HandInfo<CardPossibilityTable>& hand) {
        std::set<int> useless;
        std::map<Card, int> seen;

        for (int i=0; i < (int)hand.size(); ++i) {
            const auto& card_table = hand[i];
            if (card_table.probability_is_dead(view.get_board()) == 1.0) {
                useless.insert(i);
            } else {
                if (card_table.known()) {
                    Card card = card_table.get_card();
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

    HandInfo<CardPossibilityTable> take_public_info(int player) {
        HandInfo<CardPossibilityTable> result = std::move(this->public_info.at(player));
        this->public_info.erase(player);
        return result;
    }

    void return_public_info(int player, HandInfo<CardPossibilityTable> card_info) {
        this->public_info.insert(player, std::move(card_info));
    }

    const HandInfo<CardPossibilityTable>& get_my_public_info() const {
        return this->get_player_public_info(this->me);
    }

    // fn get_my_public_info_mut(&mut self) -> &mut HandInfo<CardPossibilityTable> {
    //     let me = self.me.clone();
    //     self.get_player_public_info_mut(&me)
    // }

    const HandInfo<CardPossibilityTable>& get_player_public_info(int player) const {
        return this->public_info.at(player);
    }

    HandInfo<CardPossibilityTable>& get_player_public_info_mut(int player) {
        return this->public_info.at(player);
    }

    void update_public_info_for_hint(const Hint& hint, const std::vector<bool>& matches) {
        auto& info = this->public_info.at(hint.player);
        info.update_for_hint(hint.hinted, matches);
    }

    void update_public_info_for_discard_or_play(BorrowedGameView& view, int player, int index, Card card) {
        auto new_card_table = CardPossibilityTable::from(this->public_counts);
        {
            auto& info = this->public_info.at(player);
            assert(info[index].is_possible(card));
            info.remove(index);

            // push *before* incrementing public counts
            if (info.len() < view.hand_size(player)) {
                info.push(new_card_table);
            }
        }

        // TODO: decrement weight counts for fully determined cards, ahead of time

        // note: other_player could be player, as well
        // in particular, we will decrement the newly drawn card
        for (int other_player : view.board.get_players()) {
            auto& info = this->public_info.at(other_player);
            for (auto&& card_table : info) {
                card_table.decrement_weight_if_possible(card);
            }
        }

        this->public_counts.increment(card);
    }

    HandInfo<CardPossibilityTable> get_private_info(const OwnedGameView& view) const {
        auto info = this->get_my_public_info();
        for (auto& card_table : info) {
            for (auto&& kv : view.other_hands) {
                for (card in kv.second) {
                    card_table.decrement_weight_if_possible(card);
                }
            }
        }
        return info;
    }

    int get_hint_index_score(const CardPossibilityTable& card_table, const OwnedGameView& view) const {
        if (card_table.probability_is_dead(view.get_board()) == 1.0) {
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

    int get_index_for_hint(const HandInfo<CardPossibilityTable>& info, const OwnedGameView& view) const {
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
    double hint_goodness(const Hinted& hinted, int hint_player, const OwnedGameView& view) const {
        auto hand = view.get_hand(hint_player);

        // get post-hint hand_info
        auto hand_info = self.get_player_public_info(hint_player);
        int total_info  = 3 * (view.board.num_players - 1);
        auto questions = this->get_questions(total_info, view, hand_info);
        for (auto&& question : questions) {
            auto answer = question->answer(hand, view);
            question->acknowledge_answer(answer, hand_info, view);
        }

        double goodness = 1.0;
        for (int i=0; i < (int)hand_info.size(); ++i) {
            auto& card_table = hand_info[i];
            auto card = hand[i];
            if (card_table.probability_is_dead(view.board) == 1.0) {
                continue;
            }
            if (card_table.is_determined()) {
                continue;
            }
            auto old_weight = card_table.total_weight();
            if (hinted.kind == Hinted::Color) {
                card_table.mark_color(color, color == card.color);
            } else if (hinted.kind == Hinted::Value) {
                card_table.mark_value(value, value == card.value);
            } else {
                assert(false);
            }
            auto new_weight = card_table.total_weight();
            assert(new_weight <= old_weight);
            double bonus = (
                card_table.is_determined() ? 2 :
                card_table.probability_is_dead(view.board) == 1.0 ? 2 :
                1
            );
            goodness *= bonus * (old_weight / new_weight);
        }
        return goodness;
    }

    // Returns the number of ways to hint the player.
    int get_info_per_player(int player) const {
        auto info = this->get_player_public_info(player);

        bool may_be_all_one_color = false;
        for (auto k : COLORS) {
            if (std::all_of(info.begin(), info.end(), [k](auto&& card) { return card.can_be_color(k); })) {
                may_be_all_one_color = true;
                break;
            }
        }

        bool may_be_all_one_number = false;
        for (auto k : VALUES) {
            if (std::all_of(info.begin(), info.end(), [k](auto&& card) { return card.can_be_value(k); })) {
                may_be_all_one_value = true;
                break;
            }
        }

        return (!may_be_all_one_color && !may_be_all_one_number) ? 4 : 3;

        // Determine if both:
        //  - it is public that there are at least two colors
        //  - it is public that there are at least two numbers
    }

    std::vector<int> get_other_players_starting_after(int player) const {
        std::vector<int> result;
        for (int i=0; i < numPlayers; ++i) {
            result.push_back((player + 1 + i) % numPlayers);
        }
        return result;
    }

    Hinted get_best_hint_of_options(int hint_player, std::set<Hinted> hint_option_set) const {
        const auto& view = this->last_view;

        // using hint goodness barely helps
        std::vector<std::pair<double, Hinted>> hint_options;
        for (auto&& hinted : hint_option_set) {
            double goodness = self.hint_goodness(hinted, hint_player, view);
            hint_options.emplace_back(goodness, hinted);
        }

        std::sort(hint_options.begin(), hint_options.end(), [](const auto& h1, const auto& h2) {
            return h1.first > h2.first;
        });

        if (hint_options.empty()) {
            // NOTE: Technically possible, but never happens
        } else {
            if (hint_options.len() > 1) {
                // debug!("Choosing amongst hint options: {:?}", hint_options);
            }
        }

        return std::move(hint_options[0].second);
    }

    TurnChoice get_hint() const {
        const auto& view = this->last_view;

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
        int hint_info = this->get_hint_sum_info(total_info, view);

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

        auto hinted = (hint_info_we_can_give_to_this_player == 3) ? (
            (hint_type == 0) ? Hinted::Value(hint_card.value) :
            (hint_type == 1) ? Hinted::Color(hint_card.color) :
            (hint_type == 2) ? [&]() -> Hinted {
                // NOTE: this doesn't do that much better than just hinting
                // the first thing that doesn't match the hint_card
                std::set<Hinted> hint_option_set;
                for (auto&& card : hand) {
                    if (card.color != hint_card.color) hint_option_set.insert(Hinted::Color(card.color));
                    if (card.value != hint_card.value) hint_option_set.insert(Hinted::Value(card.value));
                }
                return this->get_best_hint_of_options(hint_player, hint_option_set);
            }() : []() -> Hinted {
                assert(!"Invalid hint type");
            }()
        ) : (
            (hint_type == 0) ? Hinted::Value(hint_card.value) :
            (hint_type == 1) ? Hinted::Color(hint_card.color) :
            (hint_type == 2) ? [&]() -> Hinted {
                // Any value hint for a card other than the first
                std::set<Hinted> hint_option_set;
                for (auto&& card : hand) {
                    if (card.value != hint_card.value) hint_option_set.insert(Hinted::Value(card.value));
                }
                return this->get_best_hint_of_options(hint_player, hint_option_set);
            }() :
            (hint_type == 2) ? [&]() -> Hinted {
                    // Any color hint for a card other than the first
                std::set<Hinted> hint_option_set;
                for (auto&& card : hand) {
                    if (card.color != hint_card.color) hint_option_set.insert(Hinted::Color(card.color));
                }
                return this->get_best_hint_of_options(hint_player, hint_option_set);
            }() : []() -> Hinted {
                assert(!"Invalid hint type");
            }()
        );

        return TurnChoice::Hint(hint_player, hinted);
    }

    void infer_from_hint(const Hint& hint, std::vector<bool> result) {
        int hinter = this->last_view.board.player;

        std::vector<int> info_per_player;
        int total_info = 0;
        for (int player : this->get_other_players_starting_after(hinter)) {
            int x = this->get_info_per_player(player);
            info_per_player.push_back(x);
            total_info += x;
        }

        int n = numPlayers;

        int player_amt = (n + hint.player - hinter - 1) % n;

        int amt_from_prev_players = 0;
        for (int i=0; i < player_amt; ++i) {
            amt_from_prev_players += info_per_player[i];
        }
        int hint_info_we_can_give_to_this_player = info_per_player[player_amt];

        int card_index = this->get_index_for_hint(this->get_player_public_info(hint.player), this->last_view);
        int hint_type;
        if (hint_info_we_can_give_to_this_player == 3) {
            if (result[card_index]) {
                if (hint.hinted.kind == Hinted::Value) {
                    hint_type = 0;
                } else {
                    hint_type = 1;
                }
            } else {
                hint_type = 2;
            }
        } else {
            if (result[card_index]) {
                if (hint.hinted.kind == Hinted::Value) {
                    hint_type = 0;
                } else {
                    hint_type = 1;
                }
            } else {
                if (hint.hinted.kind == Hinted::Value) {
                    hint_type = 2;
                } else {
                    hint_type = 3;
                }
            }
        }
        int hint_value = amt_from_prev_players + hint_type;

        ModulusInformation mod_info(total_info, hint_value);

        this->update_from_hint_sum(mod_info);
    }

    TurnChoice decide(const BorrowedGameView&) {
        // we already stored the view
        const auto& view = this->last_view;

        auto private_info = this->get_private_info(view);

        std::vector<std::pair<int, CardTable>> playable_cards;
        for (auto&& kv : private_info) {
            const auto& card_table = kv.second;
            if (card_table.probability_is_playable(view.board) == 1.0) {
                playable_cards.push_back(kv);
            }
        }

        if (!playable_cards.empty()) {
            // play the best playable card
            // the higher the play_score, the better to play
            double play_score = -1.0;
            int play_index = 0;

            for (const auto& kv : playable_cards) {
                int index = kv.first;
                const auto& card_table = kv.second;
                double score = this->get_average_play_score(view, card_table);
                if (score > play_score) {
                    play_score = score;
                    play_index = index;
                }
            }

            return TurnChoice::Play(play_index);
        }

        int discard_threshold =
            view.board.total_cards
            - (COLORS.size() * VALUES.size())
            - (view.board.num_players * view.board.hand_size);

        // make a possibly risky play
        // TODO: consider removing this, if we improve information transfer
        if (view.board.lives_remaining > 1 &&
            view.board.discard_size() <= discard_threshold)
        {
            std::vector<std::tuple<int, CardTable, double>> risky_playable_cards;
            for (auto&& kv : private_info) {
                auto&& card_table = kv.second;
                // card is either playable or dead
                if (!card_table.probability_of_predicate([](Card card) {
                    return view.board.is_playable(card) || view.board.is_dead(card);
                }) == 1.0) {
                    continue;
                }
                double p = card_table.probability_is_playable(view.board);
                risky_playable_cards.emplace_back(kv.first, card_table, p);
            }

            if (!risky_playable_cards.empty()) {
                std::sort(risky_playable_cards.begin(), risky_playable_cards.end(), [&](const auto& a, const auto& b) {
                    return std::get<2>(a) > std::get<2>(b);
                });

                auto maybe_play = risky_playable_cards[0];
                if (std::get<2>(maybe_play) > 0.75) {
                    return TurnChoice::Play(std::get<0>(maybe_play));
                }
            }
        }

        auto public_useless_indices = this->find_useless_cards(view, this->get_my_public_info());
        auto useless_indices = this->find_useless_cards(view, private_info);

        if (view.board.discard_size() <= discard_threshold) {
            // if anything is totally useless, discard it
            if (public_useless_indices.size() > 1) {
                auto info = self.get_hint_sum_info(public_useless_indices.size(), view);
                return TurnChoice::Discard(public_useless_indices[info.value]);
            } else if (!useless_indices.empty()) {
                // TODO: have opponents infer that i knew a card was useless
                // TODO: after that, potentially prefer useless indices that arent public
                return TurnChoice::Discard(useless_indices[0]);
            }
        }

        // hinting is better than discarding dead cards
        // (probably because it stalls the deck-drawing).
        if (view.board.hints_remaining > 0) {
            if (view.someone_else_can_play()) {
                return self.get_hint();
            }
        }

        // if anything is totally useless, discard it
        if (public_useless_indices.size() > 1) {
            auto info = self.get_hint_sum_info(public_useless_indices.size(), view);
            return TurnChoice::Discard(public_useless_indices[info.value]);
        } else if (!useless_indices.empty()) {
            return TurnChoice::Discard(useless_indices[0]);
        }

        // NOTE: the only conditions under which we would discard a potentially useful card:
        // - we have no known useless cards
        // - there are no hints remaining OR nobody else can play

        // Play the best discardable card
        double compval = 0.0;
        int index = 0;
        for (int i=0; i < (int)private_info.size(); ++i) {
            const auto& card_table = private_info[i];
            double probability_is_seen = card_table.probability_of_predicate([](Card card) {
                return view.can_see(card);
            });
            double my_compval =
                20.0 * probability_is_seen
                + 10.0 * card_table.probability_is_dispensable(view.board)
                + card_table.average_value();

            if (my_compval > compval) {
                compval = my_compval;
                index = i;
            }
        }
        return TurnChoice::Discard(index);
    }

    void update(const TurnRecord& turn_record, const BorrowedGameView& view) {
        switch (turn_record.choice) {
            case TurnChoice::Hint: {
                auto& hint = turn_record.hint;
                auto& matches = turn_record.matches;
                this->infer_from_hint(hint, matches);
                this->update_public_info_for_hint(hint, matches);
            }
            case TurnChoice::Discard: {
                int index = turn_record.card_index;
                auto known_useless_indices = self.find_useless_cards(
                    this->last_view, this->get_player_public_info(turn_record.player)
                );

                if (known_useless_indices.size() > 1) {
                    // unwrap is safe because *if* a discard happened, and there were known
                    // dead cards, it must be a dead card
                    int value = -1;
                    for (int i=0; i < (int)known_useless_indices.size(); ++i) {
                        if (known_useless_indices[i] == index) {
                            value = i;
                            break;
                        }
                    }
                    assert(value != -1);
                    this->update_from_hint_sum(ModulusInformation(known_useless_indices.size(), value));
                }

                Card card = server.activeCard();
                this->update_public_info_for_discard_or_play(view, turn_record.player, index, card);
            }
            case TurnChoice::Play: {
                int index = turn_record.card_index;
                this->update_public_info_for_discard_or_play(view, turn_record.player, index, card);
            }
        }
        this->last_view = OwnedGameView::clone_from(view);
    }
};
