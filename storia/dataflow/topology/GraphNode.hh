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
        typedef
            std::function<void(GraphNode*, update_arg_type)> update_func_type;
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
            update_func_(dest_, update);
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

    template <typename N>
    void subscribe_to(N* publisher, Edge::update_func_type update_func) {
        static_assert(
                std::is_base_of_v<GraphNode, N>,
                "Publisher must be a GraphNode.");
        auto bound_upquery_func =
            std::bind(&N::receive_upquery, publisher, std::placeholders::_1);
        auto edge = std::make_shared<Edge>(
                publisher, this, update_func, bound_upquery_func);
        publisher->add_subscribing_edge(edge);
        publishers_.insert(edge);
    }

    void receive_upquery(std::shared_ptr<Upquery> upquery) {
        (void)upquery;
    }

protected:
    void add_subscribing_edge(std::shared_ptr<Edge> edge) {
        subscribers_.insert(edge);
    }

    void propagate(std::shared_ptr<Update> update) {
        for (auto subscriber : subscribers_) {
            subscriber->send_update(update);
        }
    }

    std::set<std::shared_ptr<Edge>> publishers_;
    std::set<std::shared_ptr<Edge>> subscribers_;
};

typedef GraphNode::Edge GraphEdge;

};  // namespace storia
