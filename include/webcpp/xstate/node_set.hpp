// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 A set of state nodes that keeps the order they were added in, as the
 JavaScript Set XState keeps its active nodes in
 (doc: #xstate-invariant-10).

 Tip: the order is not cosmetic: it decides which eventless transition a
 parallel region takes first and the order of a state value's keys.
*/
#ifndef WEBCPP_XSTATE_NODE_SET_HPP
#define WEBCPP_XSTATE_NODE_SET_HPP

#include <algorithm>
#include <cstddef>
#include <vector>

namespace webcpp::xstate {

class node_set {
public:
    node_set() = default;

    template <class Range>
    explicit node_set(const Range& nodes) {
        for (const std::size_t node : nodes) {
            insert(node);
        }
    }

    /** Adds `node` at the end, unless it is already in the set. */
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

    /** Removes `node`, the others keeping their order. */
    void erase(std::size_t node) {
        if (!contains(node)) {
            return;
        }
        present_[node] = false;
        order_.erase(std::ranges::find(order_, node));
    }

    [[nodiscard]] bool contains(std::size_t node) const noexcept {
        return node < present_.size() && present_[node];
    }

    [[nodiscard]] std::size_t size() const noexcept { return order_.size(); }

    [[nodiscard]] bool empty() const noexcept { return order_.empty(); }

    /** The node at `index` in insertion order. */
    [[nodiscard]] std::size_t at(std::size_t index) const { return order_.at(index); }

    [[nodiscard]] auto begin() const noexcept { return order_.begin(); }

    [[nodiscard]] auto end() const noexcept { return order_.end(); }

    [[nodiscard]] const std::vector<std::size_t>& ordered() const noexcept { return order_; }

private:
    std::vector<std::size_t> order_;
    std::vector<bool> present_;
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_NODE_SET_HPP
