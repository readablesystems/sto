#pragma once

#include "GraphNode.hh"

namespace storia {

namespace nodes {

template <typename K, typename T, typename B=T, typename U=size_t>
class CountNode : public GraphNode {
public:
    class entry_type : public Entry {
    public:
        entry_type() : key_(), value_(0) {}
        entry_type(const U& count) : key_(), value_(count) {}
        entry_type(const K& key, const U& count) : key_(key), value_(count) {}

        U operator = (const U& value) {
            value_ = value;
            return value_;
        }

        U operator += (const U& value) {
            value_ += value;
            return value_;
        }

        K key() const {
            return key_;
        }

        void set_key(const K& key) {
            key_ = key;
        }

        U value() const {
            return value_;
        }

    private:
        K key_;
        U value_;
    };

    typedef K key_type;
    typedef Count<T, B, entry_type> op_type;

private:
    typedef typename op_type::input_type input_type;
    typedef typename op_type::output_type output_type;

public:
    CountNode(const op_type& count) : count_(count) {}

    template <typename... Args>
    CountNode(Args&&... args) : count_(std::forward<Args>(args)...) {}

    std::optional<output_type> get(const key_type& key) {
        return table_.get(key);
    }

    static void receive(GraphNode* gnode, std::shared_ptr<Update> update_p) {
        auto node = reinterpret_cast<CountNode*>(gnode);
        auto update = static_cast<input_type*>(update_p.get());
        node->receive_update(*update);
    }

private:
    void receive_update(input_type& update_in) {
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

    op_type count_;
    Table<key_type, entry_type> table_;
};

};  // namespace nodes

using nodes::CountNode;

};  // namespace storia
