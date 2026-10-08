// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 A snapshot of a machine: where it is, its context, its status and output,
 and the active nodes it was computed from; and the tree queries that
 compute a state value from active nodes (State.ts, stateUtils.ts).

 @see "The snapshot", in the guide.
 @see "A snapshot is a value", in the guide.
*/
#ifndef WEBCPP_XSTATE_SNAPSHOT_HPP
#define WEBCPP_XSTATE_SNAPSHOT_HPP

#include <webcpp/xstate/errors.hpp>
#include <webcpp/xstate/machine.hpp>
#include <webcpp/xstate/node_set.hpp>
#include <webcpp/xstate/state_value.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace webcpp::xstate {

/**
 A snapshot's status.

 It ports XState's `snapshot.status`. XState's fourth status, `stopped`, has
 no counterpart here: XState's macrostep gives it, through `transition` too,
 to a snapshot that receives the event `xstate.stop`, while xstate runs
 `xstate.stop` as any other event. For an actor of the actor layer,
 `actor_system::status_of` tells it, as an `xactor::status`.

 @see "The snapshot", in the guide.
*/
enum class status {
    /** The machine runs. */
    active,

    /** The machine reached a final state at the top level, with its output, when it has one. */
    done,

    /** A macrostep failed, the snapshot's `error` saying why. */
    error,
};

/**
 Where a machine is, as a plain value to keep, copy or store.

 It ports XState's machine snapshot. xstate makes snapshots and keeps none:
 a caller keeps the one a macrostep settled in and runs the next event from
 it. A default-constructed snapshot is no machine's; start from
 @ref get_initial_snapshot or a cursor.

 @see "The snapshot", in the guide.
 @see "A snapshot is a value", in the guide.
 @see "Persisted state", in the guide.
*/
struct snapshot {
    /**
     XState's state value, computed from @ref nodes.

     The keys of each of its objects are in the order JavaScript enumerates
     them in XState's value: those that are array indices first, by number,
     then the others in the order of @ref nodes.
    */
    boost::json::value value{};

    /** The context. */
    boost::json::value context{};

    /** Whether the machine runs, is done, or failed. */
    enum status status = status::active;

    /** The machine's output once it is done; absent when it has none, JavaScript's `undefined`. */
    std::optional<boost::json::value> output{};

    /** Why the status is @ref status::error; an empty code otherwise. */
    boost::system::error_code error{};

    /**
     With the error @ref errc::actor_failed, the failed actor's error, when
     its error event carried one.
    */
    std::optional<boost::json::value> error_value{};

    /**
     The active nodes, by index, in XState's order.

     A node kept active keeps its place, an exited one leaves, and entered
     ones are appended in document order. The order decides which eventless
     transition a parallel region takes first, and the order of the keys of
     the state value that are not array indices. A microstep that leaves the
     machine done lists the nodes by descending `order`, each after its
     descendants, because XState sorts its array of active nodes in place
     when the machine is done. @ref resolve_state sorts nothing, done or not,
     as XState's `resolveState` does not.

     @see "Guarantees", in the guide: guarantee 10.
    */
    std::vector<std::size_t> nodes{};

    /** What each history node remembers, by its id, in recording order. */
    std::vector<std::pair<std::string, std::vector<std::size_t>>> history{};

    /** The tags of the active nodes, each once, in the order of @ref nodes. */
    std::vector<std::string> tags{};

    /**
     The child actors, `(id, src)`, that the microsteps spawned, for an
     invoke or a @ref spawn_child_action, and have not stopped since.

     They are listed as JavaScript enumerates XState's `children` object: ids
     that are array indices first, by number, then the others in spawning
     order.

     @see "Guarantees", in the guide: guarantee 18.
    */
    std::vector<std::pair<std::string, std::string>> children{};

    /**
     Whether the snapshot is in a state value.

     It ports XState's `snapshot.matches`: @ref matches_state with the
     snapshot's @ref value as the child.

     @param state_value A state value, or a dotted string.
     @return `true` when the snapshot is in `state_value` or a state within it.
    */
    [[nodiscard]] bool matches(const boost::json::value& state_value) const {
        return matches_state(state_value, value);
    }

    /**
     Whether an active node carries a tag.

     It ports XState's `snapshot.hasTag`.

     @param tag The tag.
     @return `true` when @ref tags holds `tag`.
    */
    [[nodiscard]] bool has_tag(std::string_view tag) const {
        return std::ranges::find(tags, tag) != tags.end();
    }

    /**
     What a history node remembers.

     @param history_id The history node's id.
     @return A pointer into @ref history, or `nullptr` when the node has not
     recorded.
    */
    [[nodiscard]] const std::vector<std::size_t>* remembered(std::string_view history_id) const {
        for (const auto& [id, nodes_remembered] : history) {
            if (id == history_id) {
                return &nodes_remembered;
            }
        }
        return nullptr;
    }
};

namespace detail {

/**
 Adds the child `id`, or gives the one already there its new `src`, where
 JavaScript keeps the key in XState's children object
 (doc: #xstate-invariant-18).
*/
inline void set_child(snapshot& of, std::string id, std::string src) {
    const auto existing =
        std::ranges::find_if(of.children, [&id](const auto& child) { return child.first == id; });
    if (existing != of.children.end()) {
        existing->second = std::move(src);
        return;
    }
    auto position = of.children.end();
    if (const std::optional<std::uint64_t> index = array_index_of(id); index.has_value()) {
        position = std::ranges::find_if(of.children, [&index](const auto& child) {
            const std::optional<std::uint64_t> other = array_index_of(child.first);
            return !other.has_value() || *other > *index;
        });
    }
    of.children.emplace(position, std::move(id), std::move(src));
}

/** Removes the child `id`; whether there was one. */
inline bool remove_child(snapshot& of, std::string_view id) {
    return std::erase_if(of.children, [id](const auto& child) { return child.first == id; }) > 0;
}

/** An atomic or a final node; XState's isAtomicStateNode. */
inline bool is_atomic(const machine& owner, std::size_t node) {
    const node_type type = owner.node(node).type;
    return type == node_type::atomic || type == node_type::final;
}

/** A node's children that are not history nodes; XState's getChildren. */
inline std::vector<std::size_t> children_of(const machine& owner, std::size_t node) {
    std::vector<std::size_t> children;
    for (const auto& [key, child] : owner.node(node).states) {
        if (owner.node(child).type != node_type::history) {
            children.push_back(child);
        }
    }
    return children;
}

/**
 The ancestors of `node`, nearest first, up to `to` excluded or to the root
 included; XState's getProperAncestors.
*/
inline std::vector<std::size_t> proper_ancestors(const machine& owner, std::size_t node,
                                                 std::optional<std::size_t> to) {
    std::vector<std::size_t> ancestors;
    if (to.has_value() && *to == node) {
        return ancestors;
    }
    std::optional<std::size_t> marker = owner.node(node).parent;
    while (marker.has_value() && marker != to) {
        ancestors.push_back(*marker);
        marker = owner.node(*marker).parent;
    }
    return ancestors;
}

/**
 Whether `child` is a descendant of `parent`; XState's isDescendant, for
 which every node descends from no node at all.
*/
inline bool is_descendant(const machine& owner, std::size_t child,
                          std::optional<std::size_t> parent) {
    std::optional<std::size_t> above = owner.node(child).parent;
    while (above.has_value() && above != parent) {
        above = owner.node(*above).parent;
    }
    return above == parent;
}

/**
 The nodes a node starts in: itself, and down through each compound's
 initial child and each parallel's children, in that order; XState's
 getInitialStateNodes.
*/
inline node_set initial_state_nodes(const machine& owner, std::size_t node) {
    node_set initial;
    std::vector<std::size_t> pending{node};
    while (!pending.empty()) {
        const std::size_t next = pending.back();
        pending.pop_back();
        if (initial.contains(next)) {
            continue;
        }
        initial.insert(next);
        const state_node& visited = owner.node(next);
        if (visited.type == node_type::compound && visited.initial.target.has_value() &&
            !visited.initial.target->empty()) {
            pending.push_back(visited.initial.target->front());
        } else if (visited.type == node_type::parallel) {
            const std::vector<std::size_t> children = children_of(owner, next);
            pending.insert(pending.end(), children.rbegin(), children.rend());
        }
    }
    return initial;
}

/** XState's getInitialStateNodesWithTheirAncestors. */
inline node_set initial_with_ancestors(const machine& owner, std::size_t node) {
    node_set states = initial_state_nodes(owner, node);
    for (std::size_t index = 0; index < states.size(); ++index) {
        for (const std::size_t ancestor : proper_ancestors(owner, states.at(index), node)) {
            states.insert(ancestor);
        }
    }
    return states;
}

/**
 The full set of active nodes a partial one implies: a compound with no
 active child enters its initial ones, a parallel enters each region not
 yet active, and every ancestor is active; XState's getAllStateNodes.

 @note Which nodes already have an active child is decided once, from the
 nodes given, as XState decides it before it adds any.
*/
inline node_set all_state_nodes(const machine& owner, const std::vector<std::size_t>& nodes) {
    node_set all(nodes);
    std::vector<bool> has_active_child(owner.size(), false);
    for (const std::size_t node : nodes) {
        if (const std::optional<std::size_t> parent = owner.node(node).parent) {
            has_active_child[*parent] = true;
        }
    }
    for (std::size_t index = 0; index < all.size(); ++index) {
        const std::size_t node = all.at(index);
        const node_type type = owner.node(node).type;
        if (type == node_type::compound && !has_active_child[node]) {
            for (const std::size_t entered : initial_with_ancestors(owner, node)) {
                all.insert(entered);
            }
        } else if (type == node_type::parallel) {
            for (const std::size_t child : children_of(owner, node)) {
                if (all.contains(child)) {
                    continue;
                }
                for (const std::size_t entered : initial_with_ancestors(owner, child)) {
                    all.insert(entered);
                }
            }
        }
    }
    for (std::size_t index = 0; index < all.size(); ++index) {
        std::optional<std::size_t> marker = owner.node(all.at(index)).parent;
        while (marker.has_value()) {
            all.insert(*marker);
            marker = owner.node(*marker).parent;
        }
    }
    return all;
}

/**
 The state value of a set of active nodes; XState's getStateValue, whose
 recursion over the adjacency list is a list of the values still to fill.

 @note XState builds a JavaScript object, so an object value's keys are
 written in the order JavaScript enumerates them, array indices first.
*/
inline boost::json::value state_value_of(const machine& owner,
                                         const std::vector<std::size_t>& nodes) {
    const node_set all = all_state_nodes(owner, nodes);
    std::map<std::size_t, std::vector<std::size_t>> adjacency;
    for (const std::size_t node : all) {
        adjacency.try_emplace(node);
        if (const std::optional<std::size_t> parent = owner.node(node).parent) {
            adjacency[*parent].push_back(node);
        }
    }
    boost::json::value root;
    std::vector<std::pair<std::size_t, boost::json::value*>> pending{{0, &root}};
    while (!pending.empty()) {
        const auto [node, slot] = pending.back();
        pending.pop_back();
        const auto children = adjacency.find(node);
        if (children == adjacency.end() || children->second.empty()) {
            *slot = boost::json::object{};
            continue;
        }
        const std::size_t first = children->second.front();
        if (owner.node(node).type == node_type::compound && is_atomic(owner, first)) {
            *slot = boost::json::string(owner.node(first).key);
            continue;
        }
        boost::json::object& value = slot->emplace_object();
        std::vector<std::size_t> keyed = children->second;
        std::ranges::stable_sort(keyed, [&owner](std::size_t left, std::size_t right) {
            return enumerated_before(owner.node(left).key, owner.node(right).key);
        });
        for (const std::size_t child : keyed) {
            value[owner.node(child).key] = nullptr;
        }
        for (const std::size_t child : children->second) {
            pending.emplace_back(child, &value[owner.node(child).key]);
        }
    }
    return root;
}

/**
 Whether `node` is in a final state among `active`: a compound whose final
 child is active, a parallel all of whose regions are; XState's
 isInFinalState.
*/
inline bool is_in_final_state(const machine& owner, const node_set& active, std::size_t node) {
    std::vector<std::size_t> pending{node};
    while (!pending.empty()) {
        const std::size_t next = pending.back();
        pending.pop_back();
        const node_type type = owner.node(next).type;
        if (type == node_type::compound) {
            const std::vector<std::size_t> children = children_of(owner, next);
            const bool any_final = std::ranges::any_of(children, [&](std::size_t child) {
                return owner.node(child).type == node_type::final && active.contains(child);
            });
            if (!any_final) {
                return false;
            }
        } else if (type == node_type::parallel) {
            const std::vector<std::size_t> children = children_of(owner, next);
            pending.insert(pending.end(), children.begin(), children.end());
        } else if (type != node_type::final) {
            return false;
        }
    }
    return true;
}

/**
 A snapshot of `nodes` with the rest given; its value and tags computed
 from the nodes; XState's createMachineSnapshot.
*/
inline snapshot make_snapshot(
    const machine& owner, std::vector<std::size_t> nodes, boost::json::value context,
    enum status state, std::optional<boost::json::value> output,
    std::vector<std::pair<std::string, std::vector<std::size_t>>> history) {
    snapshot made{
        .value = state_value_of(owner, nodes),
        .context = std::move(context),
        .status = state,
        .output = std::move(output),
        .nodes = std::move(nodes),
        .history = std::move(history),
    };
    for (const std::size_t node : made.nodes) {
        for (const std::string& tag : owner.node(node).tags) {
            if (!made.has_tag(tag)) {
                made.tags.push_back(tag);
            }
        }
    }
    return made;
}

/** Recomputes a snapshot's value and tags after its nodes changed. */
inline void refresh(const machine& owner, snapshot& changed) {
    changed.value = state_value_of(owner, changed.nodes);
    changed.tags.clear();
    for (const std::size_t node : changed.nodes) {
        for (const std::string& tag : owner.node(node).tags) {
            if (!changed.has_tag(tag)) {
                changed.tags.push_back(tag);
            }
        }
    }
}

/**
 The nodes a state value names under `node`, as XState lists them, an
 object value listing the root, the node and its children, in the order
 JavaScript enumerates its keys, before the nodes under each child;
 XState's getStateNodes.
*/
inline result<std::vector<std::size_t>> state_nodes_of(const machine& owner, std::size_t node,
                                                       const boost::json::value& state_value) {
    std::vector<std::size_t> listed;
    std::vector<std::pair<std::size_t, const boost::json::value*>> pending{{node, &state_value}};
    while (!pending.empty()) {
        const auto [visited, value] = pending.back();
        pending.pop_back();
        if (value->is_string()) {
            const result<std::size_t> child = owner.child(visited, value->get_string());
            if (!child.has_value()) {
                return failure<std::vector<std::size_t>>(errc::unknown_state);
            }
            listed.push_back(visited);
            listed.push_back(*child);
            continue;
        }
        if (!value->is_object()) {
            return failure<std::vector<std::size_t>>(errc::unknown_state);
        }
        listed.push_back(0);
        listed.push_back(visited);
        std::vector<std::pair<std::size_t, const boost::json::value*>> children;
        for (const boost::json::key_value_pair* member : enumerated(value->get_object())) {
            const result<std::size_t> child = member->key().starts_with("#")
                                                  ? owner.node_by_id(member->key())
                                                  : owner.child(visited, member->key());
            if (!child.has_value()) {
                return failure<std::vector<std::size_t>>(errc::unknown_state);
            }
            listed.push_back(*child);
            children.emplace_back(*child, &member->value());
        }
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }
    return listed;
}

}  // namespace detail

/**
 The `meta` of every active node that has one, by node id.

 It ports XState's `snapshot.getMeta()`. The members are in the order of
 `of.nodes`.

 @param owner The machine.
 @param of A snapshot of `owner`.
 @return The object of the active nodes' `meta`, keyed by node id.
 @pre `of` is a snapshot of `owner`.

 @see "Meta and descriptions", in the guide.
*/
inline boost::json::object get_meta(const machine& owner, const snapshot& of) {
    boost::json::object meta;
    for (const std::size_t node : of.nodes) {
        if (const std::optional<boost::json::value>& own = owner.node(node).meta) {
            meta[owner.node(node).id] = *own;
        }
    }
    return meta;
}

/**
 Builds the snapshot of a state value, completed to every node it implies,
 with a context.

 It ports XState's `machine.resolveState({ value, context })`. The value is
 completed with a compound state's initial child, a parallel state's
 regions and every ancestor. The snapshot's status is @ref status::done when
 the value is in a final state at the top level and @ref status::active
 otherwise; it has no output, no history and no children. Its
 @ref snapshot::nodes are in the order XState's `resolveState` gives them,
 which for a parallel state differs from the order a microstep gives: each
 level's named children before their descendants, so the done value
 `{"a": "a2", "b": "b2"}` of a parallel root `p` gives `p`, `p.a`, `p.b`,
 `p.a.a2`, `p.b.b2`.

 The keys of an object value are read in the order JavaScript enumerates
 them, as XState's `getStateNodes` reads `Object.keys`: of a parallel root
 `p` with the regions `"3"` and `"z"`, the value `{"z": "z2", "3": "d"}`
 gives `p.3` before `p.z`, and the snapshot's value is
 `{"3": "d", "z": "z2"}`, in both libraries. A string value names a child of
 the root by its key, as in XState, so `"a.b"` is the key `a.b`, not a path;
 a nested value is written as an object.

 @param owner The machine.
 @param state_value The state value, a string or an object.
 @param context The snapshot's context.
 @return The snapshot; @ref errc::unknown_state when the value names a
 state the machine does not have, or is neither a string nor an object.

 @see "Persisted state", in the guide.
*/
inline result<snapshot> resolve_state(const machine& owner, const boost::json::value& state_value,
                                      boost::json::value context) {
    const result<std::vector<std::size_t>> named = detail::state_nodes_of(owner, 0, state_value);
    if (!named.has_value()) {
        return named.error();
    }
    const boost::json::value resolved =
        detail::state_value_of(owner, detail::all_state_nodes(owner, *named).ordered());
    const result<std::vector<std::size_t>> renamed = detail::state_nodes_of(owner, 0, resolved);
    if (!renamed.has_value()) {
        return renamed.error();
    }
    const node_set nodes = detail::all_state_nodes(owner, *renamed);
    const enum status state =
        detail::is_in_final_state(owner, nodes, 0) ? status::done : status::active;
    return detail::make_snapshot(owner, nodes.ordered(), std::move(context), state, std::nullopt,
                                 {});
}

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_SNAPSHOT_HPP
