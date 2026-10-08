// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 A set of state nodes that keeps the order they were added in, as the
 JavaScript Set XState keeps its active nodes in
 (doc: #xstate-invariant-10).

 @note The order is not cosmetic: it decides which eventless transition a
 parallel region takes first and the order of a state value's keys.

 @see "The active state nodes", in the guide.
*/
#ifndef WEBCPP_XSTATE_NODE_SET_HPP
#define WEBCPP_XSTATE_NODE_SET_HPP

#include <algorithm>
#include <cstddef>
#include <vector>

namespace webcpp::xstate {

/**
 A set of node indices that keeps the order they were added in.

 It stands for the JavaScript `Set` in which XState keeps its active nodes,
 which enumerates its members in insertion order. The order is not
 cosmetic: it decides which eventless transition a parallel region takes
 first, and the order of the keys of a state value that are not array
 indices. xstate computes active nodes with it, and @ref snapshot::nodes
 holds its @ref ordered list.

 @see "The active state nodes", in the guide.
 @see "Guarantees", in the guide.
*/
class node_set {
public:
    /** Makes an empty set. */
    node_set() = default;

    /**
     Makes the set of the nodes of a range, in the range's order.

     @tparam Range A range of `std::size_t`.
     @param nodes The nodes, each inserted as @ref insert inserts it.
    */
    template <class Range>
    explicit node_set(const Range& nodes) {
        for (const std::size_t node : nodes) {
            insert(node);
        }
    }

    /**
     Adds a node at the end, unless it is already in the set.

     @param node The node's index in its machine.
    */
    void insert(std::size_t node) {
        if (contains(node)) {
            return;
        }
        if (node >= present_.size()) {
            present_.resize(node + 1, false);
        }
        present_[node] = true;
        order_.push_back(node);
    }

    /**
     Removes a node, the others keeping their order.

     A node not in the set is no error: nothing changes.

     @param node The node's index in its machine.
    */
    void erase(std::size_t node) {
        if (!contains(node)) {
            return;
        }
        present_[node] = false;
        order_.erase(std::ranges::find(order_, node));
    }

    /**
     Whether a node is in the set.

     @param node The node's index in its machine.
     @return `true` when the set holds `node`.
    */
    [[nodiscard]] bool contains(std::size_t node) const noexcept {
        return node < present_.size() && present_[node];
    }

    /**
     How many nodes the set holds.

     @return The number of nodes.
    */
    [[nodiscard]] std::size_t size() const noexcept { return order_.size(); }

    /**
     Whether the set holds no node.

     @return `true` when @ref size is 0.
    */
    [[nodiscard]] bool empty() const noexcept { return order_.empty(); }

    /**
     The node at a position of the insertion order.

     @param index The position, from 0.
     @return The node at `index`.
     @pre `index < size()`.
    */
    [[nodiscard]] std::size_t at(std::size_t index) const { return order_.at(index); }

    /**
     The first node, in insertion order.

     @return An iterator over the nodes, as @ref ordered lists them.
    */
    [[nodiscard]] auto begin() const noexcept { return order_.begin(); }

    /**
     The end of the nodes, in insertion order.

     @return The iterator past the last node.
    */
    [[nodiscard]] auto end() const noexcept { return order_.end(); }

    /**
     The nodes, in insertion order.

     @return The list of the nodes, valid until the set changes.
    */
    [[nodiscard]] const std::vector<std::size_t>& ordered() const noexcept { return order_; }

private:
    std::vector<std::size_t> order_;
    std::vector<bool> present_;
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_NODE_SET_HPP
