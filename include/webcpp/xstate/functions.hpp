// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 XState's pure functions, each the macrostep cursor run to its end
 (transition.ts, getNextSnapshot.ts).

 Tip: these run without a fuel bound, so a machine whose eventless
 transitions never settle never returns here; a caller that must bound the
 work drives the cursor itself (doc: #xstate-invariant-8).
*/
#ifndef WEBCPP_XSTATE_FUNCTIONS_HPP
#define WEBCPP_XSTATE_FUNCTIONS_HPP

#include <webcpp/xstate/actions.hpp>
#include <webcpp/xstate/event.hpp>
#include <webcpp/xstate/machine.hpp>
#include <webcpp/xstate/macrostep.hpp>
#include <webcpp/xstate/snapshot.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace webcpp::xstate {

namespace detail {

/** Runs a cursor to its end, keeping every microstep. */
inline std::vector<microstep> run_to_end(macrostep cursor) {
    std::vector<microstep> steps;
    while (!cursor.done()) {
        steps.push_back(cursor.next().step);
    }
    return steps;
}

/** Runs a cursor to its end, keeping only what settled. */
inline macrostep_result settle(macrostep cursor) {
    while (!cursor.done()) {
        cursor.next();
    }
    return cursor.result();
}

}  // namespace detail

/** The initial macrostep's microsteps; XState's getInitialMicrosteps. */
inline std::vector<microstep> get_initial_microsteps(const machine& owner,
                                                     const boost::json::value& input = nullptr) {
    return detail::run_to_end(begin_initial(owner, input));
}

/** The snapshot a machine starts in; XState's getInitialSnapshot. */
inline snapshot get_initial_snapshot(const machine& owner,
                                     const boost::json::value& input = nullptr) {
    return detail::settle(begin_initial(owner, input)).snapshot;
}

/** The initial snapshot and its actions; XState's initialTransition. */
inline std::pair<snapshot, std::vector<action>> initial_transition(
    const machine& owner, const boost::json::value& input = nullptr) {
    macrostep_result settled = detail::settle(begin_initial(owner, input));
    return {std::move(settled.snapshot), std::move(settled.actions)};
}

/** The microsteps an event runs; XState's getMicrosteps. */
inline std::vector<microstep> get_microsteps(const machine& owner, const snapshot& from,
                                             const event& happened) {
    return detail::run_to_end(begin(owner, from, happened));
}

/** The snapshot an event leads to and its actions; XState's transition. */
inline std::pair<snapshot, std::vector<action>> transition(const machine& owner,
                                                           const snapshot& from,
                                                           const event& happened) {
    macrostep_result settled = detail::settle(begin(owner, from, happened));
    return {std::move(settled.snapshot), std::move(settled.actions)};
}

/** The snapshot an event leads to; XState's getNextSnapshot. */
inline snapshot get_next_snapshot(const machine& owner, const snapshot& from,
                                  const event& happened) {
    return detail::settle(begin(owner, from, happened)).snapshot;
}

/**
 Whether an event would take a transition that changes something, a target
 or an action, from a snapshot; XState's snapshot.can.
*/
inline result<bool> can(const machine& owner, const snapshot& from, const event& happened) {
    const result<detail::transition_list> selected =
        detail::select_transitions(owner, happened, from);
    if (!selected.has_value()) {
        return selected.error();
    }
    return std::ranges::any_of(*selected, [](const transition_definition* one) {
        return one->target.has_value() || !one->actions.empty();
    });
}

/**
 Every transition a snapshot could take, guards not evaluated: each active
 atomic node's and its ancestors', each node once, its event transitions
 then its eventless ones; XState's getNextTransitions.
*/
inline std::vector<const transition_definition*> get_next_transitions(const machine& owner,
                                                                      const snapshot& from) {
    std::vector<const transition_definition*> potential;
    std::vector<bool> visited(owner.size(), false);
    for (const std::size_t atomic : from.nodes) {
        if (!detail::is_atomic(owner, atomic)) {
            continue;
        }
        std::vector<std::size_t> chain{atomic};
        const std::vector<std::size_t> ancestors =
            detail::proper_ancestors(owner, atomic, std::nullopt);
        chain.insert(chain.end(), ancestors.begin(), ancestors.end());
        for (const std::size_t node : chain) {
            if (visited[node]) {
                continue;
            }
            visited[node] = true;
            for (const auto& [descriptor, transitions] : owner.node(node).transitions) {
                for (const transition_definition& one : transitions) {
                    potential.push_back(&one);
                }
            }
            for (const transition_definition& one : owner.node(node).always) {
                potential.push_back(&one);
            }
        }
    }
    return potential;
}

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_FUNCTIONS_HPP
