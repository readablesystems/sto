#pragma once

#include "GraphNode.hh"

namespace storia {

namespace nodes {

template <
    typename K, typename TLeft, typename TRight, typename S /* synthesis */>
class InnerJoinNode : public GraphNode {
public:
    class entry_type : public Entry {
    private:
        typedef std::optional<TLeft> opt_left_type;
        typedef std::optional<TRight> opt_right_type;

    public:
        entry_type() : key_(), value_() {}
        entry_type(const S& value) : key_(), value_(value) {}
        entry_type(const K& key, const S& value) : key_(key), value_(value) {}

    private:
        entry_type(const opt_left_type& left, const opt_right_type& right)
            : key_(), value_(std::make_pair(left, right)) {}

    public:

        static entry_type from_left(TLeft& left) {
            return entry_type(left, std::nullopt);
        }

        U operator = (const U& value) {
            value_ = value;
            return value_;
        }

        K key() const {
            return key_;
        }

        void set_key(const K& key) {
            key_ = key;
        }

        S value() const {
            return value_;
        }

    private:
        K key_;
        std::variant<S, std::pair<opt_left_type, opt_right_type>> value_;
    };

    typedef K key_type;
    typedef InnerJoin<TLeft, TRight, entry_type> op_type;

private:
    typedef typename op_type::input_type input_type;
    typedef typename op_type::output_type output_type;

public:
    InnerJoinNode(const op_type& value) : op_(value) {}

    template <typename... Args>
    InnerJoinNode(Args&&... args) : count_(std::forward<Args>(args)...) {}

    std::optional<output_type> get(const key_type& key) {
        return table_.get(key);
    }

private:
    void apply(std::shared_ptr<Update> update_in_p) override {
        auto& update_in = *static_cast<input_type*>(update_in_p.get());
        auto update_out_opt = count_.consume(update_in);
        if (update_out_opt) {
            auto key = update_in.template key<key_type>();
            auto inc_value = update_out_opt->value().value();
            PartialBlindWrite<key_type, entry_type> update(
                    key,
                    [inc_value] (entry_type& entry) -> entry_type& {
                        entry += inc_value;
                        return entry;
                    });

            // Create base value first, if needed
            auto existing = table_.get(key);
            if (!existing) {
                BlindWrite<entry_type> update(
                        entry_type(key, inc_value), false);
                if (!table_.apply(update)) {
                    table_.apply(update);
                }
            } else {
                table_.apply(update);
            }

            propagate(std::shared_ptr<Update>(&update));
        }
    }

    op_type op_;
    Table<key_type, entry_type> table_;
};

};  // namespace nodes

using nodes::InnerJoinNode;

};  // namespace storia
