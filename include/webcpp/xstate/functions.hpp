// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 XState's pure functions, each the macrostep cursor run to its end
 (transition.ts, getNextSnapshot.ts).

 Like the cursor, they return no error: a machine that fails comes back in
 a snapshot whose status is `error`. `transition` and `initial_transition`
 then return, with the failed snapshot, the actions of the microsteps that
 completed before the failure, where XState's `transition` throws and its
 `initialTransition` returns as well those the failing microstep resolved
 before the failure. A caller checks the snapshot's status before it
 executes them.

 @note These run without a fuel bound, so a machine whose eventless
 transitions never settle never returns here; a caller that must bound the
 work drives the cursor itself (doc: #xstate-invariant-8).

 @see "Pure transitions and the macrostep cursor", in the guide.
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

/**
 Runs a machine's initial macrostep and returns every microstep.

 It ports XState's `getInitialMicrosteps`: @ref begin_initial run to its
 end, without a fuel bound.

 @param owner The machine.
 @param input The machine's input, null for none.
 @return Every microstep of the initial macrostep, in order.

 @see "The microsteps of the initial state", in the guide.
*/
inline std::vector<microstep> get_initial_microsteps(const machine& owner,
                                                     const boost::json::value& input = nullptr) {
    return detail::run_to_end(begin_initial(owner, input));
}

/**
 Runs a machine's initial macrostep and returns the snapshot it starts in.

 It ports XState's `getInitialSnapshot`: @ref begin_initial run to its end,
 without a fuel bound. A machine whose initial macrostep fails comes back
 in a snapshot whose status is @ref status::error.

 @param owner The machine.
 @param input The machine's input, null for none.
 @return The settled snapshot.

 @see "Computing the next state", in the guide.
*/
inline snapshot get_initial_snapshot(const machine& owner,
                                     const boost::json::value& input = nullptr) {
    return detail::settle(begin_initial(owner, input)).snapshot;
}

/**
 Runs a machine's initial macrostep and returns its snapshot and its
 actions.

 It ports XState's `initialTransition`: @ref begin_initial run to its end,
 without a fuel bound. The actions are @ref macrostep_result::actions. A
 caller checks the snapshot's status before it executes them: of an
 initial macrostep that failed, XState's actor has executed none.

 @param owner The machine.
 @param input The machine's input, null for none.
 @return The settled snapshot, and every action of the macrostep.

 @see "Computing the next state", in the guide.
 @see "Actions", in the guide.
*/
inline std::pair<snapshot, std::vector<action>> initial_transition(
    const machine& owner, const boost::json::value& input = nullptr) {
    macrostep_result settled = detail::settle(begin_initial(owner, input));
    return {std::move(settled.snapshot), std::move(settled.actions)};
}

/**
 Runs the macrostep of an event and returns every microstep.

 It ports XState's `getMicrosteps`: @ref begin run to its end, without a
 fuel bound.

 @param owner The machine.
 @param from The snapshot the macrostep begins from.
 @param happened The event.
 @return Every microstep of the macrostep, in order.
 @pre `from` is a snapshot of `owner`.

 @see "The microsteps of an event", in the guide.
*/
inline std::vector<microstep> get_microsteps(const machine& owner, const snapshot& from,
                                             const event& happened) {
    return detail::run_to_end(begin(owner, from, happened));
}

/**
 Runs the macrostep of an event and returns the snapshot it leads to and its
 actions.

 It ports XState's `transition`: @ref begin run to its end, without a fuel
 bound. A caller checks the snapshot's status before it executes the
 actions: of a macrostep that failed, XState's actor has executed only the
 custom actions and logs.

 @param owner The machine.
 @param from The snapshot the macrostep begins from.
 @param happened The event.
 @return The settled snapshot, and every action of the macrostep.
 @pre `from` is a snapshot of `owner`.

 @see "Computing the next state", in the guide.
 @see "Actions", in the guide.
*/
inline std::pair<snapshot, std::vector<action>> transition(const machine& owner,
                                                           const snapshot& from,
                                                           const event& happened) {
    macrostep_result settled = detail::settle(begin(owner, from, happened));
    return {std::move(settled.snapshot), std::move(settled.actions)};
}

/**
 Runs the macrostep of an event and returns the snapshot it leads to.

 It ports XState's `getNextSnapshot`: @ref begin run to its end, without a
 fuel bound.

 @param owner The machine.
 @param from The snapshot the macrostep begins from.
 @param happened The event.
 @return The settled snapshot.
 @pre `from` is a snapshot of `owner`.

 @see "The snapshot alone", in the guide.
*/
inline snapshot get_next_snapshot(const machine& owner, const snapshot& from,
                                  const event& happened) {
    return detail::settle(begin(owner, from, happened)).snapshot;
}

/**
 Whether an event would take, from a snapshot, a transition that does
 something.

 It ports XState's `snapshot.can(event)`: a transition does something when
 it has a target or actions. It evaluates the guards, as XState does, and
 runs no action.

 @param owner The machine.
 @param from The snapshot.
 @param happened The event.
 @return Whether a transition that does something would be taken; a
 guard's failure, or @ref errc::unknown_state when the value of `from`, or
 a `stateIn` guard's `#id`, names no state of the machine.
 @pre `from` is a snapshot of `owner`.

 @see "Whether an event would do anything", in the guide.
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
 Lists every transition a snapshot could take, guards not evaluated.

 It ports XState's `getNextTransitions`. For each active atomic node, it
 lists its own transitions and its ancestors', each node once: a node's
 event transitions first, in the order of @ref state_node::transitions, and
 its eventless ones after.

 @param owner The machine.
 @param from The snapshot.
 @return The transitions, as pointers into the machine.
 @pre `from` is a snapshot of `owner`.

 @see "Every transition a snapshot could take", in the guide.
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
