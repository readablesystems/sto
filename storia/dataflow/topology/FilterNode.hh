#pragma once

#include "GraphNode.hh"

namespace storia {

namespace nodes {

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

    static void receive(GraphNode* gnode, std::shared_ptr<Update> update_p) {
        auto node = reinterpret_cast<FilterNode*>(gnode);
        auto update = static_cast<input_type*>(update_p.get());
        node->receive_update(*update);
    }

private:
    void receive_update(input_type& update_in) {
        auto update_out_opt = filter_.consume(update_in);
        if (update_out_opt) {
            propagate(std::shared_ptr<Update>(&*update_out_opt));
        }
    }

    op_type filter_;
};

};  // namespace nodes

using nodes::FilterNode;

};  // namespace storia
