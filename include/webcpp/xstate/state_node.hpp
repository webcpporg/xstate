// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The plain data of a machine's tree: its state nodes, their transitions, and
 the actions and guards the config names, as XState's StateNode holds them
 once initialised (StateNode.ts, stateUtils.ts formatTransition).

 Tip: a node is named by its index in the machine, which is its position in
 document order, so the tree holds no pointer and copies as a value.
*/
#ifndef WEBCPP_XSTATE_STATE_NODE_HPP
#define WEBCPP_XSTATE_STATE_NODE_HPP

#include <webcpp/xstate/implementations.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace webcpp::xstate {

enum class node_type { atomic, compound, parallel, final, history };

/** A node's `history`: none when falsy, deep for "deep", shallow for any other value. */
enum class history_kind { none, shallow, deep };

/**
 An action as the config names it, or one of the two XState adds to a
 state with `after`: the delayed raise of its entry and the cancel of its
 exit (getDelayedTransitions).
*/
struct action_ref {
    enum class kind { named, after_raise, after_cancel };

    kind kind = kind::named;
    // The name, or, for the two `after` actions, the after event's type,
    // which is also the id of the delayed raise.
    std::string type{};
    // Null when the config gave no params.
    boost::json::value params{};
    // The action as the config wrote it, for the definition.
    boost::json::value config{};
    // The delay of an after_raise.
    std::optional<delay_ref> delay{};
};

/**
 A guard as the config names it: a name with its params, or one of XState's
 higher-order guards in their JSON form (doc: #differences-guards).

 Tip: the guards of a machine are one flat list and a higher-order guard
 names its operands by index, so a guard nested as deep as its JSON is held
 and evaluated without recursion.
*/
struct guard_node {
    enum class kind { named, negation, conjunction, disjunction, state_in };

    kind kind = kind::named;
    std::string type{};
    boost::json::value params{};
    std::vector<std::size_t> operands{};
    boost::json::value state_value{};
    // The guard as the config wrote it, for the definition.
    boost::json::value config{};
};

/**
 One transition of a node: XState's TransitionDefinition.

 Tip: `target` empty of value is a targetless transition, which XState tells
 apart from one whose target list is empty.
*/
struct transition_definition {
    std::size_t source = 0;
    std::optional<std::vector<std::size_t>> target{};
    std::vector<action_ref> actions{};
    // The index of the transition's guard in the machine's guards.
    std::optional<std::size_t> guard{};
    bool reenter = false;
    // The event descriptor: "" for an eventless transition, the after event
    // for a delayed one.
    std::string event_type{};
    std::optional<delay_ref> delay{};
    // The transition's config, which XState spreads into its definition.
    boost::json::object config{};
};

/**
 A child actor a state invokes, which lives while the state is active;
 XState's InvokeDefinition (StateNode.ts invoke).

 Tip: `src` names an implementation; the library runs no child, it only
 returns the spawnChild and stopChild its caller executes.
*/
struct invoke_definition {
    // The config's id, or XState's `<index>.<state id>`.
    std::string id{};
    std::string src{};
    // Absent when the config gave none, which XState leaves undefined.
    std::optional<boost::json::value> input{};
    std::optional<std::string> system_id{};
    // The invoke as the config wrote it, for its transitions and the
    // definition.
    boost::json::object config{};
};

struct state_node {
    std::string key{};
    std::string id{};
    std::optional<std::size_t> parent{};
    std::vector<std::string> path{};
    node_type type = node_type::atomic;
    history_kind history = history_kind::none;
    // The children, by key, in the order JavaScript enumerates the config's keys.
    std::vector<std::pair<std::string, std::size_t>> states{};
    std::vector<action_ref> entry{};
    std::vector<action_ref> exit{};
    // Every node has one; a node that is not compound has no target.
    transition_definition initial{};
    // By event descriptor, in XState's Map order: `on`, then `onDone`, then
    // each invoke's `onDone`, `onError` and `onSnapshot`, then the delayed
    // transitions.
    std::vector<std::pair<std::string, std::vector<transition_definition>>> transitions{};
    std::vector<transition_definition> always{};
    std::vector<invoke_definition> invoke{};
    // A history node's default target, resolved; empty when it has none.
    std::optional<std::vector<std::size_t>> history_target{};
    std::vector<std::string> tags{};
    std::optional<boost::json::value> meta{};
    std::optional<boost::json::value> output{};
    std::optional<std::string> description{};
    // XState's order: the number of distinct ids registered before this node.
    std::size_t order = 0;
    boost::json::object config{};
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_STATE_NODE_HPP
