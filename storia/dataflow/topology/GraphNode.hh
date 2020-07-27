#pragma once

#include <set>
#include <type_traits>

#include "dataflow/Logics.hh"
#include "dataflow/Operators.hh"
#include "dataflow/State.hh"

namespace storia {

class GraphNode {
public:
    class Edge {
    public:
        typedef std::shared_ptr<Update> update_arg_type;
        typedef std::function<void(update_arg_type)> update_func_type;
        typedef std::shared_ptr<Upquery> upquery_arg_type;
        typedef std::function<void(upquery_arg_type)> upquery_func_type;

        // The edge points from src to dest, with src being the upstream parent
        // and dest being the downstream child in the graph.
        Edge(
                GraphNode* src, GraphNode* dest,
                update_func_type update_func, upquery_func_type upquery_func)
            : dest_(dest), src_(src),
              update_func_(update_func),
              upquery_func_(upquery_func) {}

        void delete_edge() {
            auto sptr = std::shared_ptr<Edge>(this);

            {
                auto itr = src_->subscribers_.find(sptr);
                if (itr != src_->subscribers_.end()) {
                    src_->subscribers_.erase(itr);
                }
            }

            {
                auto itr = dest_->publishers_.find(sptr);
                if (itr != dest_->publishers_.end()) {
                    dest_->publishers_.erase(itr);
                }
            }
        }

        void send_update(update_arg_type update) {
            update_func_(update);
        }

        void send_upquery(upquery_arg_type upquery) {
            upquery_func_(upquery);
        }

    private:
        GraphNode* dest_;
        GraphNode* src_;
        update_func_type update_func_;
        upquery_func_type upquery_func_;
    };

    GraphNode() : subscribers_() {}

    template <typename N, typename F>
    void subscribe_to(N* publisher, F unbound_update_func) {
        static_assert(
                std::is_base_of_v<GraphNode, N>,
                "Publisher must be a GraphNode.");
        auto bound_update_func =
            std::bind(unbound_update_func, this, std::placeholders::_1);
        auto bound_upquery_func =
            std::bind(&N::receive_upquery, publisher, std::placeholders::_1);
        auto edge = std::make_shared<Edge>(
                publisher, this, bound_update_func, bound_upquery_func);
        publisher->add_subscribing_edge(edge);
        publishers_.insert(edge);
    }

    void receive_update(std::shared_ptr<Update> update) {
        apply(update);
    }

    void receive_upquery(std::shared_ptr<Upquery> upquery) {
        (void)upquery;
    }

protected:
    void add_subscribing_edge(std::shared_ptr<Edge> edge) {
        subscribers_.insert(edge);
    }

    virtual void apply(std::shared_ptr<Update> update) = 0;

    void propagate(std::shared_ptr<Update> update) {
        for (auto subscriber : subscribers_) {
            subscriber->send_update(update);
        }
    }

    std::set<std::shared_ptr<Edge>> publishers_;
    std::set<std::shared_ptr<Edge>> subscribers_;
};

typedef GraphNode::Edge GraphEdge;

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
