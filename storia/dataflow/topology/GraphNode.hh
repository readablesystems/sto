#pragma once

#include <type_traits>

#include "dataflow/Logics.hh"
#include "dataflow/Operators.hh"
#include "dataflow/State.hh"

namespace storia {

class GraphNode {
public:
    GraphNode() : subscribers_() {}

    void add_subscriber(GraphNode* subscriber) {
        subscribers_.push_back(std::shared_ptr<GraphNode>(subscriber));
    }

    void receive(std::shared_ptr<Update> update) {
        apply(update);
    }

protected:
    virtual void apply(std::shared_ptr<Update> update) = 0;

    void propagate(std::shared_ptr<Update> update) {
        for (auto subscriber : subscribers_) {
            subscriber->receive(update);
        }
    }

    std::vector<std::shared_ptr<GraphNode>> subscribers_;
};

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

    op_type count_;
    Table<key_type, entry_type> table_;
};

template <typename K, typename T, typename B=void>
class FilterNode : public GraphNode {
public:
    typedef K key_type;
    typedef Filter<T, B> op_type;

private:
    typedef typename op_type::input_type input_type;
    typedef typename op_type::output_type output_type;
    typedef typename op_type::update_type update_type;

public:
    FilterNode(const op_type& filter) : filter_(filter) {}

    template <typename... Args>
    FilterNode(Args&&... args) : filter_(std::forward<Args>(args)...) {}

private:
    void apply(std::shared_ptr<Update> update_ptr) override {
        auto& update_in = *static_cast<input_type*>(update_ptr.get());
        auto update_out_opt = filter_.consume(update_in);
        if (update_out_opt) {
            propagate(std::shared_ptr<Update>(&*update_out_opt));
        }
    }

    op_type filter_;
};

};  // namespace nodes

};  // namespace storia
