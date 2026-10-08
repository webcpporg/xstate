// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 One microstep of a machine, the SCXML procedure XState runs: the
 transitions an event selects, the conflicts among them, the nodes they exit
 and enter, the history they record and the done events they raise
 (stateUtils.ts, https://www.w3.org/TR/scxml/#microstepProcedure).

 @note Every function here is a port of the stateUtils.ts function its
 comment names; XState's recursion is an explicit list or stack, so a
 machine as deep as its JSON never exhausts the stack.
*/
#ifndef WEBCPP_XSTATE_MICROSTEP_HPP
#define WEBCPP_XSTATE_MICROSTEP_HPP

#include <webcpp/xstate/actions.hpp>
#include <webcpp/xstate/errors.hpp>
#include <webcpp/xstate/event.hpp>
#include <webcpp/xstate/guards.hpp>
#include <webcpp/xstate/machine.hpp>
#include <webcpp/xstate/node_set.hpp>
#include <webcpp/xstate/snapshot.hpp>
#include <webcpp/xstate/state_node.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace webcpp::xstate::detail {

/** Transitions of a machine, as pointers into it, in the order they were selected. */
using transition_list = std::vector<const transition_definition*>;

/** What each history node remembers, by its id, as @ref snapshot::history holds it. */
using history_value = std::vector<std::pair<std::string, std::vector<std::size_t>>>;

/** What a history node remembers in `history`, or nothing. */
inline const std::vector<std::size_t>* remembered_in(const history_value& history,
                                                     std::string_view id) {
    for (const auto& [history_id, nodes] : history) {
        if (history_id == id) {
            return &nodes;
        }
    }
    return nullptr;
}

/**
 Whether an event type matches a transition's descriptor: equal, the
 wildcard, or a prefix ending in ".*"; XState's matchesEventDescriptor.
*/
inline bool matches_event_descriptor(std::string_view event_type, std::string_view descriptor) {
    if (descriptor == event_type || descriptor == wildcard) {
        return true;
    }
    if (!descriptor.ends_with(".*")) {
        return false;
    }
    std::size_t descriptor_at = 0;
    std::size_t event_at = 0;
    while (true) {
        const std::size_t descriptor_end = descriptor.find('.', descriptor_at);
        const std::string_view token =
            descriptor.substr(descriptor_at, descriptor_end - descriptor_at);
        const bool last = descriptor_end == std::string_view::npos;
        if (token == "*") {
            return last;
        }
        const std::size_t event_end = event_at == std::string_view::npos
                                          ? std::string_view::npos
                                          : event_type.find('.', event_at);
        const std::string_view event_token =
            event_at == std::string_view::npos ? std::string_view()
                                               : event_type.substr(event_at, event_end - event_at);
        if (event_at == std::string_view::npos || token != event_token) {
            return false;
        }
        if (last) {
            return true;
        }
        descriptor_at = descriptor_end + 1;
        event_at = event_end == std::string_view::npos ? std::string_view::npos : event_end + 1;
    }
}

/**
 The transitions of `node` an event type selects among: the exact ones,
 then those of every matching wildcard descriptor, the longest first;
 XState's getCandidates.
*/
inline transition_list candidates_of(const machine& owner, std::size_t node,
                                     std::string_view type) {
    transition_list exact;
    std::vector<const std::pair<std::string, std::vector<transition_definition>>*> wildcards;
    for (const auto& entry : owner.node(node).transitions) {
        if (entry.first == type) {
            for (const transition_definition& one : entry.second) {
                exact.push_back(&one);
            }
        } else if (matches_event_descriptor(type, entry.first)) {
            wildcards.push_back(&entry);
        }
    }
    std::ranges::stable_sort(wildcards, [](const auto* left, const auto* right) {
        return left->first.size() > right->first.size();
    });
    for (const auto* entry : wildcards) {
        for (const transition_definition& one : entry->second) {
            exact.push_back(&one);
        }
    }
    return exact;
}

/**
 The first transition of `node` whose guard passes for the event; XState's
 StateNode.next.
*/
inline result<std::optional<const transition_definition*>> next_of(const machine& owner,
                                                                   std::size_t node,
                                                                   const snapshot& current,
                                                                   const event& happened) {
    for (const transition_definition* candidate : candidates_of(owner, node, happened.type)) {
        if (!candidate->guard.has_value()) {
            return std::optional<const transition_definition*>(candidate);
        }
        const result<bool> passed =
            evaluate_guard(owner, *candidate->guard, current.context, happened, current);
        if (!passed.has_value()) {
            return passed.error();
        }
        if (*passed) {
            return std::optional<const transition_definition*>(candidate);
        }
    }
    return std::optional<const transition_definition*>();
}

/**
 The node a key of a state value names under `node`: an id, or the child
 with that key; XState's getStateNode.
*/
inline result<std::size_t> state_node_named(const machine& owner, std::size_t node,
                                            std::string_view key) {
    if (key.starts_with('#')) {
        return owner.node_by_id(key);
    }
    return owner.child(node, key);
}

/**
 A node of the walk over a state value: the node, the children the value
 names under it with their own values, how many have been walked, and what
 they selected.
*/
struct selection_frame {
    /** The node the frame walks. */
    std::size_t node{};

    /** The children the value names under the node, each with its own value. */
    std::vector<std::pair<std::size_t, const boost::json::value*>> children{};

    /** How many of the children have been walked. */
    std::size_t next_child = 0;

    /** The transitions the walked children selected. */
    transition_list selected{};
};

/** Whether a region's value is one XState skips: null, "", or false. */
inline bool is_falsy(const boost::json::value& value) {
    return value.is_null() || (value.is_string() && value.get_string().empty()) ||
           (value.is_bool() && !value.get_bool());
}

/**
 The frame of `node` under `value`: no child for an atomic node, the named
 child for a string, every region whose value XState does not skip for an
 object of several keys, the only one for an object of one.
*/
inline result<selection_frame> frame_of(const machine& owner, std::size_t node,
                                        const boost::json::value* value) {
    selection_frame made{.node = node};
    if (value == nullptr) {
        return made;
    }
    if (value->is_string()) {
        const result<std::size_t> child = state_node_named(owner, node, value->get_string());
        if (!child.has_value()) {
            return child.error();
        }
        made.children.emplace_back(*child, nullptr);
        return made;
    }
    if (!value->is_object()) {
        return failure<selection_frame>(errc::unknown_state);
    }
    const boost::json::object& regions = value->get_object();
    for (const auto& region : regions) {
        if (regions.size() > 1 && is_falsy(region.value())) {
            continue;
        }
        const result<std::size_t> child = state_node_named(owner, node, region.key());
        if (!child.has_value()) {
            return child.error();
        }
        made.children.emplace_back(*child, &region.value());
    }
    return made;
}

/**
 The transitions an event selects in a snapshot, deepest first: a node's own
 only when no node under it selected any; XState's transitionNode with
 transitionAtomicNode, transitionCompoundNode and transitionParallelNode,
 walked depth first in the state value's order.
*/
inline result<transition_list> select_transitions(const machine& owner, const event& happened,
                                                  const snapshot& current) {
    result<selection_frame> root = frame_of(owner, 0, &current.value);
    if (!root.has_value()) {
        return root.error();
    }
    std::vector<selection_frame> stack{std::move(*root)};
    transition_list finished;
    while (!stack.empty()) {
        selection_frame& top = stack.back();
        if (top.next_child < top.children.size()) {
            const auto [child, value] = top.children[top.next_child++];
            result<selection_frame> expanded = frame_of(owner, child, value);
            if (!expanded.has_value()) {
                return expanded.error();
            }
            stack.push_back(std::move(*expanded));
            continue;
        }
        transition_list selected = std::move(top.selected);
        if (selected.empty()) {
            const result<std::optional<const transition_definition*>> own =
                next_of(owner, top.node, current, happened);
            if (!own.has_value()) {
                return own.error();
            }
            if (const std::optional<const transition_definition*> chosen = *own;
                chosen.has_value()) {
                selected.push_back(*chosen);
            }
        }
        stack.pop_back();
        transition_list& into = stack.empty() ? finished : stack.back().selected;
        into.insert(into.end(), selected.begin(), selected.end());
    }
    return finished;
}

/**
 A history node's default transition targets, and whether they are its
 parent's initial transition; XState's resolveHistoryDefaultTransition.
 unknown_target for a history at the root, unless its target is an empty
 list: XState reads the default from a parent the root has none of, and
 throws.
*/
inline result<std::pair<std::vector<std::size_t>, bool>> history_default_of(const machine& owner,
                                                                            std::size_t node) {
    const state_node& history = owner.node(node);
    if (history.history_target.has_value()) {
        return std::pair{*history.history_target, false};
    }
    if (!history.parent.has_value()) {
        return failure<std::pair<std::vector<std::size_t>, bool>>(errc::unknown_target);
    }
    const std::size_t parent = *history.parent;
    if (owner.node(parent).type == node_type::parallel) {
        return std::pair{std::vector<std::size_t>{parent}, false};
    }
    return std::pair{owner.node(parent).initial.target.value_or(std::vector<std::size_t>{}), true};
}

/**
 The nodes a transition targets, a history node standing for what it
 remembers or for its default targets; XState's getEffectiveTargetStates.
*/
inline result<std::vector<std::size_t>> effective_target_states(const machine& owner,
                                                                const transition_definition& taken,
                                                                const history_value& history) {
    node_set targets;
    if (!taken.target.has_value()) {
        return {};
    }
    std::vector<std::size_t> pending(taken.target->rbegin(), taken.target->rend());
    while (!pending.empty()) {
        const std::size_t node = pending.back();
        pending.pop_back();
        if (owner.node(node).type != node_type::history) {
            targets.insert(node);
            continue;
        }
        if (const std::vector<std::size_t>* remembered =
                remembered_in(history, owner.node(node).id)) {
            for (const std::size_t one : *remembered) {
                targets.insert(one);
            }
            continue;
        }
        const result<std::pair<std::vector<std::size_t>, bool>> defaults =
            history_default_of(owner, node);
        if (!defaults.has_value()) {
            return defaults.error();
        }
        pending.insert(pending.end(), defaults->first.rbegin(), defaults->first.rend());
    }
    return targets.ordered();
}

/**
 The nearest proper ancestor of the first node that every other node
 descends from; XState's findLeastCommonAncestor.
*/
inline std::optional<std::size_t> least_common_ancestor(const machine& owner,
                                                        const std::vector<std::size_t>& nodes) {
    for (const std::size_t ancestor : proper_ancestors(owner, nodes.front(), std::nullopt)) {
        const bool common = std::all_of(nodes.begin() + 1, nodes.end(), [&](std::size_t node) {
            return is_descendant(owner, node, ancestor);
        });
        if (common) {
            return ancestor;
        }
    }
    return std::nullopt;
}

/**
 The node a transition exits and enters within: its source when every
 target is inside it and it does not reenter, otherwise the least common
 ancestor, the root, or no node for a root transition that reenters;
 XState's getTransitionDomain.
*/
inline result<std::optional<std::size_t>> transition_domain(const machine& owner,
                                                            const transition_definition& taken,
                                                            const history_value& history) {
    result<std::vector<std::size_t>> effective = effective_target_states(owner, taken, history);
    if (!effective.has_value()) {
        return effective.error();
    }
    std::vector<std::size_t> targets = std::move(*effective);
    const bool inside = std::ranges::all_of(targets, [&](std::size_t target) {
        return target == taken.source || is_descendant(owner, target, taken.source);
    });
    if (!taken.reenter && inside) {
        return taken.source;
    }
    targets.push_back(taken.source);
    if (const std::optional<std::size_t> ancestor = least_common_ancestor(owner, targets)) {
        return ancestor;
    }
    if (taken.reenter) {
        return std::optional<std::size_t>();
    }
    return std::optional<std::size_t>(0);
}

/**
 The active nodes the transitions exit, in the order found; XState's
 computeExitSet.
*/
inline result<std::vector<std::size_t>> compute_exit_set(const machine& owner,
                                                         const transition_list& taken,
                                                         const node_set& active,
                                                         const history_value& history) {
    node_set exited;
    for (const transition_definition* one : taken) {
        if (!one->target.has_value() || one->target->empty()) {
            continue;
        }
        const result<std::optional<std::size_t>> found = transition_domain(owner, *one, history);
        if (!found.has_value()) {
            return found.error();
        }
        const std::optional<std::size_t> domain = *found;
        if (one->reenter && domain == one->source) {
            exited.insert(*domain);
        }
        for (const std::size_t node : active) {
            if (is_descendant(owner, node, domain)) {
                exited.insert(node);
            }
        }
    }
    return exited.ordered();
}

/**
 The transitions left once conflicting ones are removed: of two that exit a
 common node, one whose source is a descendant of the other's replaces it,
 otherwise the earlier one stays; XState's removeConflictingTransitions.
*/
inline result<transition_list> remove_conflicting(const machine& owner,
                                                  const transition_list& enabled,
                                                  const node_set& active,
                                                  const history_value& history) {
    transition_list filtered;
    for (const transition_definition* first : enabled) {
        if (std::ranges::find(filtered, first) != filtered.end()) {
            continue;
        }
        const result<std::vector<std::size_t>> first_exits =
            compute_exit_set(owner, {first}, active, history);
        if (!first_exits.has_value()) {
            return first_exits.error();
        }
        bool preempted = false;
        transition_list removed;
        for (const transition_definition* second : filtered) {
            const result<std::vector<std::size_t>> second_exits =
                compute_exit_set(owner, {second}, active, history);
            if (!second_exits.has_value()) {
                return second_exits.error();
            }
            const bool intersect = std::ranges::any_of(*first_exits, [&](std::size_t node) {
                return std::ranges::find(*second_exits, node) != second_exits->end();
            });
            if (!intersect) {
                continue;
            }
            if (is_descendant(owner, first->source, second->source)) {
                removed.push_back(second);
            } else {
                preempted = true;
                break;
            }
        }
        if (preempted) {
            continue;
        }
        std::erase_if(filtered, [&](const transition_definition* one) {
            return std::ranges::find(removed, one) != removed.end();
        });
        filtered.push_back(first);
    }
    return filtered;
}

/**
 The first eventless transition whose guard passes, of `atomic` or of its
 nearest ancestor that has one; nothing when none passes.
*/
inline result<std::optional<const transition_definition*>> first_eventless(const machine& owner,
                                                                           std::size_t atomic,
                                                                           const snapshot& current,
                                                                           const event& happened) {
    std::vector<std::size_t> chain{atomic};
    const std::vector<std::size_t> ancestors = proper_ancestors(owner, atomic, std::nullopt);
    chain.insert(chain.end(), ancestors.begin(), ancestors.end());
    for (const std::size_t node : chain) {
        for (const transition_definition& one : owner.node(node).always) {
            if (!one.guard.has_value()) {
                return std::optional<const transition_definition*>(&one);
            }
            const result<bool> passed =
                evaluate_guard(owner, *one.guard, current.context, happened, current);
            if (!passed.has_value()) {
                return passed.error();
            }
            if (*passed) {
                return std::optional<const transition_definition*>(&one);
            }
        }
    }
    return std::optional<const transition_definition*>();
}

/**
 The eventless transitions a snapshot enables: for each atomic node, the
 first passing one of it or of its nearest ancestor that has any; XState's
 selectEventlessTransitions.
*/
inline result<transition_list> select_eventless(const machine& owner, const snapshot& current,
                                                const event& happened) {
    transition_list enabled;
    for (const std::size_t atomic : current.nodes) {
        if (!is_atomic(owner, atomic)) {
            continue;
        }
        const result<std::optional<const transition_definition*>> found =
            first_eventless(owner, atomic, current, happened);
        if (!found.has_value()) {
            return found.error();
        }
        const std::optional<const transition_definition*> chosen = *found;
        if (chosen.has_value() && std::ranges::find(enabled, *chosen) == enabled.end()) {
            enabled.push_back(*chosen);
        }
    }
    return remove_conflicting(owner, enabled, node_set(current.nodes), current.history);
}

/**
 The entry set of a microstep: the nodes to enter, and among them those
 entered by default, whose initial actions run; XState's computeEntrySet
 with addDescendantStatesToEnter, addAncestorStatesToEnter and
 addProperAncestorStatesToEnter, whose mutual recursion is a stack of the
 calls still to run, in the order XState makes them.
*/
class entry_set {
public:
    /** Makes an empty entry set for a machine, with the history it reads. */
    entry_set(const machine& owner, const history_value& history)
        : owner_(owner), history_(history) {}

    /** The nodes to enter, in the order XState adds them. */
    node_set to_enter;

    /** The nodes among them entered by default, whose initial actions run. */
    node_set for_default_entry;

    /**
     XState's computeEntrySet, for the transitions of one microstep;
     unknown_target where a history's default cannot be resolved
     (history_default_of).
    */
    result<void> compute(const transition_list& taken) {
        for (const transition_definition* one : taken) {
            if (const result<void> computed = compute_one(*one); !computed.has_value()) {
                return computed;
            }
        }
        return {};
    }

private:
    // addDescendantStatesToEnter(node).
    struct descend {
        std::size_t node{};
    };

    // For each node from `next`: enter it, mark `default_parent` for default
    // entry when there is one, and descend into it.
    struct enter_each {
        std::vector<std::size_t> nodes{};
        std::size_t next = 0;
        std::optional<std::size_t> default_parent{};
    };

    // For each node from `next`: addProperAncestorStatesToEnter(node, to).
    struct proper_ancestors_each {
        std::vector<std::size_t> nodes{};
        std::size_t next = 0;
        std::optional<std::size_t> to{};
    };

    // addAncestorStatesToEnter(nodes, domain), from `next`.
    struct ancestors_each {
        std::vector<std::size_t> nodes{};
        std::optional<std::size_t> domain{};
        std::size_t next = 0;
    };

    // The regions of a parallel node that no node to enter is under yet,
    // from `next`, each entered and descended into.
    struct regions_each {
        std::size_t node{};
        std::size_t next = 0;
        bool by_default = false;
    };

    using task =
        std::variant<descend, enter_each, proper_ancestors_each, ancestors_each, regions_each>;

    /** What one transition adds to the entry set: its targets, their descendants and ancestors. */
    result<void> compute_one(const transition_definition& one) {
        const result<std::optional<std::size_t>> found = transition_domain(owner_, one, history_);
        if (!found.has_value()) {
            return found.error();
        }
        const std::optional<std::size_t> domain = *found;
        for (const std::size_t target : one.target.value_or(std::vector<std::size_t>{})) {
            const bool entered = one.source != target || domain != one.source || one.reenter;
            if (!is_history(target) && entered) {
                to_enter.insert(target);
                for_default_entry.insert(target);
            }
            if (const result<void> descended = run(descend{target}); !descended.has_value()) {
                return descended;
            }
        }
        const result<std::vector<std::size_t>> effective =
            effective_target_states(owner_, one, history_);
        if (!effective.has_value()) {
            return effective.error();
        }
        const bool root_reentry = !owner_.node(one.source).parent.has_value() && one.reenter;
        for (const std::size_t target : *effective) {
            std::vector<std::size_t> ancestors = proper_ancestors(owner_, target, domain);
            if (domain.has_value() && owner_.node(*domain).type == node_type::parallel) {
                ancestors.push_back(*domain);
            }
            if (const result<void> entered = run(ancestors_each{
                    .nodes = std::move(ancestors),
                    .domain = root_reentry ? std::nullopt : domain,
                });
                !entered.has_value()) {
                return entered;
            }
        }
        return {};
    }

    [[nodiscard]] bool is_history(std::size_t node) const {
        return owner_.node(node).type == node_type::history;
    }

    result<void> run(task first) {
        std::vector<task> stack{std::move(first)};
        while (!stack.empty() && !failed_.has_value()) {
            task next = std::move(stack.back());
            stack.pop_back();
            std::visit([this, &stack](auto& one) { step(one, stack); }, next);
        }
        if (failed_.has_value()) {
            return *failed_;
        }
        return {};
    }

    void step(const descend& call, std::vector<task>& stack) {
        const state_node& node = owner_.node(call.node);
        if (node.type == node_type::history) {
            if (const std::vector<std::size_t>* remembered = remembered_in(history_, node.id)) {
                stack.emplace_back(proper_ancestors_each{.nodes = *remembered, .to = node.parent});
                stack.emplace_back(enter_each{.nodes = *remembered});
                return;
            }
            result<std::pair<std::vector<std::size_t>, bool>> found =
                history_default_of(owner_, call.node);
            if (!found.has_value()) {
                failed_ = found.error();
                return;
            }
            auto [defaults, is_parent_initial] = std::move(*found);
            stack.emplace_back(proper_ancestors_each{.nodes = defaults, .to = node.parent});
            stack.emplace_back(enter_each{
                .nodes = std::move(defaults),
                .default_parent = is_parent_initial ? node.parent : std::nullopt,
            });
            return;
        }
        const std::vector<std::size_t> no_target;
        const std::vector<std::size_t>& initial_target =
            node.initial.target.has_value() ? *node.initial.target : no_target;
        if (node.type == node_type::compound && !initial_target.empty()) {
            const std::size_t initial = initial_target.front();
            if (!is_history(initial)) {
                to_enter.insert(initial);
                for_default_entry.insert(initial);
            }
            stack.emplace_back(proper_ancestors_each{.nodes = {initial}, .to = call.node});
            stack.emplace_back(descend{initial});
            return;
        }
        if (node.type == node_type::parallel) {
            stack.emplace_back(regions_each{.node = call.node, .by_default = true});
        }
    }

    void step(enter_each& call, std::vector<task>& stack) {
        if (call.next >= call.nodes.size()) {
            return;
        }
        const std::size_t node = call.nodes[call.next];
        to_enter.insert(node);
        if (call.default_parent.has_value()) {
            for_default_entry.insert(*call.default_parent);
        }
        ++call.next;
        stack.emplace_back(std::move(call));
        stack.emplace_back(descend{node});
    }

    void step(proper_ancestors_each& call, std::vector<task>& stack) {
        if (call.next >= call.nodes.size()) {
            return;
        }
        std::vector<std::size_t> ancestors =
            proper_ancestors(owner_, call.nodes[call.next], call.to);
        ++call.next;
        stack.emplace_back(std::move(call));
        stack.emplace_back(ancestors_each{.nodes = std::move(ancestors), .domain = std::nullopt});
    }

    void step(ancestors_each& call, std::vector<task>& stack) {
        if (call.next >= call.nodes.size()) {
            return;
        }
        const std::size_t ancestor = call.nodes[call.next];
        if (!call.domain.has_value() || is_descendant(owner_, ancestor, call.domain)) {
            to_enter.insert(ancestor);
        }
        ++call.next;
        stack.emplace_back(std::move(call));
        if (owner_.node(ancestor).type == node_type::parallel) {
            stack.emplace_back(regions_each{.node = ancestor, .by_default = false});
        }
    }

    void step(regions_each& call, std::vector<task>& stack) {
        const std::vector<std::size_t> regions = children_of(owner_, call.node);
        while (call.next < regions.size()) {
            const std::size_t region = regions[call.next];
            ++call.next;
            const bool under = std::ranges::any_of(to_enter, [&](std::size_t entered) {
                return is_descendant(owner_, entered, region);
            });
            if (under) {
                continue;
            }
            to_enter.insert(region);
            if (call.by_default) {
                for_default_entry.insert(region);
            }
            stack.emplace_back(call);
            stack.emplace_back(descend{region});
            return;
        }
    }

    const machine& owner_;
    const history_value& history_;
    // Why a step could not go on, which ends the run.
    std::optional<boost::system::error_code> failed_;
};

/**
 What a microstep produced, with whether it changed the snapshot; the error
 that ended it, if one did, after `actions` were resolved.
*/
struct microstep_outcome {
    /** The snapshot the microstep left. */
    snapshot next{};

    /** The actions it returned, or resolved before its failure. */
    std::vector<action> actions{};

    /** Whether it changed the snapshot, which re-enables eventless transitions. */
    bool changed = false;

    /** The error that ended it, if one did. */
    std::optional<boost::system::error_code> failure{};
};

/** A microstep that `error` ended, with what it had resolved before. */
inline microstep_outcome ended(microstep_outcome outcome, boost::system::error_code error) {
    outcome.failure = error;
    return outcome;
}

/** The pointers to a list of actions, for resolve_actions. */
inline std::vector<const action_ref*> pointers_to(const std::vector<action_ref>& actions) {
    std::vector<const action_ref*> pointers;
    pointers.reserve(actions.size());
    for (const action_ref& one : actions) {
        pointers.push_back(&one);
    }
    return pointers;
}

/**
 A node's output: computed by the implementations' output under its id,
 from the context and `happened`, or the config's; XState's resolveOutput.
*/
inline result<std::optional<boost::json::value>> output_of(const machine& owner,
                                                           const state_node& node,
                                                           const boost::json::value& context,
                                                           const event& happened) {
    const auto maker = owner.registry().outputs.find(node.id);
    if (maker == owner.registry().outputs.end()) {
        return node.output;
    }
    const boost::json::value no_params;
    return maker->second(action_args{.context = context, .event = happened, .params = no_params});
}

/** Whether a node has an output, fixed or computed. */
inline bool has_output(const machine& owner, const state_node& node) {
    return node.output.has_value() || owner.registry().outputs.contains(node.id);
}

/** The done event of a node, with an output when it has one. */
inline event done_event(std::string_view id, const std::optional<boost::json::value>& output) {
    event done{.type = "xstate.done.state." + std::string(id), .payload = {}};
    if (output.has_value()) {
        done.payload.emplace("output", *output);
    }
    return done;
}

/**
 The machine's output once `completion` completed it: its root's output,
 computed from the done event of `completion` with that node's output;
 XState's getMachineOutput.
*/
inline result<std::optional<boost::json::value>> machine_output(const machine& owner,
                                                                const state_node& completion,
                                                                const boost::json::value& context,
                                                                const event& happened) {
    if (!has_output(owner, owner.root())) {
        return std::optional<boost::json::value>();
    }
    std::optional<boost::json::value> completed;
    if (has_output(owner, completion) && completion.parent.has_value()) {
        result<std::optional<boost::json::value>> output =
            output_of(owner, completion, context, happened);
        if (!output.has_value()) {
            return output.error();
        }
        completed = std::move(*output);
    }
    return output_of(owner, owner.root(), context, done_event(completion.id, completed));
}

/**
 Records what each history node of an exited node remembers of the active
 nodes; whether it has any; XState's exitStates, for history.
*/
inline bool record_history(const machine& owner, std::size_t node, const node_set& active,
                           history_value& history) {
    bool recorded_any = false;
    for (const auto& [key, child] : owner.node(node).states) {
        const state_node& history_node = owner.node(child);
        if (history_node.type != node_type::history) {
            continue;
        }
        std::vector<std::size_t> recorded;
        for (const std::size_t one : active) {
            const bool deep = history_node.history == history_kind::deep;
            const bool kept = deep ? is_atomic(owner, one) && is_descendant(owner, one, node)
                                   : owner.node(one).parent == node;
            if (kept) {
                recorded.push_back(one);
            }
        }
        recorded_any = true;
        const auto existing = std::ranges::find_if(
            history, [&](const auto& entry) { return entry.first == history_node.id; });
        if (existing != history.end()) {
            existing->second = std::move(recorded);
        } else {
            history.emplace_back(history_node.id, std::move(recorded));
        }
    }
    return recorded_any;
}

/**
 Records what each history node under an exited node remembers, then, for
 each exited node, deepest first, runs its exit actions, stops the child of
 each of its invokes and removes it; XState's exitStates.
*/
inline result<void> exit_states(const machine& owner, snapshot& next, const event& happened,
                                const transition_list& taken, node_set& active,
                                history_value& history, bool& history_changed,
                                const resolution& into) {
    result<std::vector<std::size_t>> computed = compute_exit_set(owner, taken, active, history);
    if (!computed.has_value()) {
        return computed.error();
    }
    std::vector<std::size_t> exited = std::move(*computed);
    std::ranges::stable_sort(exited, [&owner](std::size_t left, std::size_t right) {
        return owner.node(left).order > owner.node(right).order;
    });
    for (const std::size_t node : exited) {
        if (record_history(owner, node, active, history)) {
            history_changed = true;
        }
    }
    for (const std::size_t node : exited) {
        if (const result<void> resolved =
                resolve_actions(owner, next, happened, pointers_to(owner.node(node).exit), into);
            !resolved.has_value()) {
            return resolved;
        }
        for (const invoke_definition& invoked : owner.node(node).invoke) {
            resolve_stop(next, invoked.id, into);
        }
        active.erase(node);
    }
    return {};
}

/**
 Marks each sendTo among a node's actions, from `first` on, that names one
 of the node's invokes by its bare id with the point XState binds it: the
 end of those actions (retryResolveSendTo, for deferredActorIds).
*/
inline void mark_deferred_sends(const state_node& entered, std::vector<action>& returned,
                                std::size_t first) {
    for (std::size_t index = first; index < returned.size(); ++index) {
        action& sent = returned[index];
        const boost::json::value* target =
            sent.params.is_object() ? sent.params.get_object().if_contains("targetId") : nullptr;
        if (!sent.builtin || sent.type != "xstate.sendTo" || target == nullptr ||
            !target->is_string()) {
            continue;
        }
        const std::string_view id = target->get_string();
        if (std::ranges::any_of(entered.invoke,
                                [id](const invoke_definition& one) { return one.id == id; })) {
            sent.bound_at = returned.size();
        }
    }
}

/**
 What entering one node runs: its entry actions, the spawn of each of its
 invokes' children, then, entered by default, its initial actions; XState's
 enterStates, for one node.
*/
inline result<void> run_entry(const machine& owner, snapshot& next, const event& happened,
                              const state_node& entered, bool by_default, const resolution& into) {
    if (const result<void> resolved = resolve_actions(
            owner, next, happened, pointers_to(entered.entry), into, entered.invoke);
        !resolved.has_value()) {
        return resolved;
    }
    for (const invoke_definition& invoked : entered.invoke) {
        std::optional<boost::json::value> input = invoked.input;
        if (const auto maker = owner.registry().inputs.find(invoked.id);
            maker != owner.registry().inputs.end()) {
            const boost::json::value context = next.context;
            const boost::json::value no_params;
            result<std::optional<boost::json::value>> made = maker->second(
                action_args{.context = context, .event = happened, .params = no_params});
            if (!made.has_value()) {
                return made.error();
            }
            input = std::move(*made);
        }
        resolve_spawn(next, invoked.id, invoked.src, input, invoked.system_id, into);
    }
    if (by_default) {
        return resolve_actions(owner, next, happened, pointers_to(entered.initial.actions), into,
                               entered.invoke);
    }
    return {};
}

/**
 Enters one node as run_entry says; a sendTo among its actions may name one
 of its invokes' children before its spawn, and is bound at the end of the
 node's actions, a point a failure never reaches.
*/
inline result<void> enter_node(const machine& owner, snapshot& next, const event& happened,
                               const state_node& entered, bool by_default, const resolution& into) {
    const std::size_t first = into.returned.size();
    const result<void> ran = run_entry(owner, next, happened, entered, by_default, into);
    mark_deferred_sends(entered, into.returned, first);
    return ran;
}

/**
 Enters the entry set in document order, each node as enter_node says; a
 final node raises its parent's done event, and its completed parallel
 ancestors', and completes the machine when it completes the root;
 XState's enterStates.
*/
inline result<void> enter_states(const machine& owner, snapshot& next, const event& happened,
                                 const transition_list& taken, node_set& active,
                                 const history_value& history, bool is_initial,
                                 const resolution& into) {
    entry_set entries(owner, history);
    if (const result<void> computed = entries.compute(taken); !computed.has_value()) {
        return computed;
    }
    if (is_initial) {
        entries.for_default_entry.insert(0);
    }
    std::vector<std::size_t> sorted = entries.to_enter.ordered();
    std::ranges::stable_sort(sorted, [&owner](std::size_t left, std::size_t right) {
        return owner.node(left).order < owner.node(right).order;
    });
    node_set completed;
    for (const std::size_t node : sorted) {
        active.insert(node);
        const state_node& entered = owner.node(node);
        if (const result<void> resolved = enter_node(
                owner, next, happened, entered, entries.for_default_entry.contains(node), into);
            !resolved.has_value()) {
            return resolved;
        }
        if (entered.type != node_type::final) {
            continue;
        }
        const std::optional<std::size_t> parent = entered.parent;
        std::optional<std::size_t> marker;
        if (parent.has_value()) {
            marker = owner.node(*parent).type == node_type::parallel ? parent
                                                                     : owner.node(*parent).parent;
        }
        std::size_t completion = node;
        if (parent.has_value() && owner.node(*parent).type == node_type::compound) {
            result<std::optional<boost::json::value>> output =
                output_of(owner, entered, next.context, happened);
            if (!output.has_value()) {
                return output.error();
            }
            into.queue.push_back(done_event(owner.node(*parent).id, *output));
        }
        while (marker.has_value() && owner.node(*marker).type == node_type::parallel &&
               !completed.contains(*marker) && is_in_final_state(owner, active, *marker)) {
            completed.insert(*marker);
            into.queue.push_back(done_event(owner.node(*marker).id, std::nullopt));
            completion = *marker;
            marker = owner.node(*marker).parent;
        }
        if (marker.has_value()) {
            continue;
        }
        result<std::optional<boost::json::value>> output =
            machine_output(owner, owner.node(completion), next.context, happened);
        if (!output.has_value()) {
            return output.error();
        }
        next.status = status::done;
        next.output = std::move(*output);
        into.changed = true;
    }
    return {};
}

/**
 One microstep: the transitions' conflicts removed, their nodes exited, their
 actions run, their nodes entered, and, when the machine is done, every
 active node's exit actions; XState's microstep.
*/
inline microstep_outcome run_microstep(const machine& owner, const transition_list& transitions,
                                       const snapshot& current, const event& happened,
                                       bool is_initial, std::deque<event>& queue) {
    microstep_outcome outcome{.next = current};
    if (transitions.empty()) {
        return outcome;
    }
    const resolution into{.queue = queue, .returned = outcome.actions, .changed = outcome.changed};
    node_set active(current.nodes);
    history_value history = current.history;
    bool history_changed = false;
    const result<transition_list> conflicts_removed =
        remove_conflicting(owner, transitions, active, history);
    if (!conflicts_removed.has_value()) {
        return ended(std::move(outcome), conflicts_removed.error());
    }
    const transition_list& filtered = *conflicts_removed;
    if (!is_initial) {
        if (const result<void> exited = exit_states(owner, outcome.next, happened, filtered, active,
                                                    history, history_changed, into);
            !exited.has_value()) {
            return ended(std::move(outcome), exited.error());
        }
    }
    std::vector<const action_ref*> transition_actions;
    for (const transition_definition* one : filtered) {
        for (const action_ref& action : one->actions) {
            transition_actions.push_back(&action);
        }
    }
    if (const result<void> resolved =
            resolve_actions(owner, outcome.next, happened, transition_actions, into);
        !resolved.has_value()) {
        return ended(std::move(outcome), resolved.error());
    }
    if (const result<void> entered = enter_states(owner, outcome.next, happened, filtered, active,
                                                  history, is_initial, into);
        !entered.has_value()) {
        return ended(std::move(outcome), entered.error());
    }
    std::vector<std::size_t> next_nodes = active.ordered();
    if (outcome.next.status == status::done) {
        // XState sorts its array of the active nodes in place here, so the
        // done snapshot lists them deepest first.
        std::ranges::stable_sort(next_nodes, [&owner](std::size_t left, std::size_t right) {
            return owner.node(left).order > owner.node(right).order;
        });
        std::vector<const action_ref*> exits;
        for (const std::size_t node : next_nodes) {
            for (const action_ref& action : owner.node(node).exit) {
                exits.push_back(&action);
            }
        }
        if (const result<void> resolved =
                resolve_actions(owner, outcome.next, happened, exits, into);
            !resolved.has_value()) {
            return ended(std::move(outcome), resolved.error());
        }
    }
    const bool same_nodes = current.nodes.size() == active.size() &&
                            std::ranges::all_of(current.nodes, [&active](std::size_t node) {
                                return active.contains(node);
                            });
    if (!history_changed && same_nodes) {
        if (outcome.changed) {
            refresh(owner, outcome.next);
        }
        return outcome;
    }
    outcome.next.nodes = std::move(next_nodes);
    outcome.next.history = std::move(history);
    refresh(owner, outcome.next);
    outcome.changed = true;
    return outcome;
}

}  // namespace webcpp::xstate::detail

#endif  // WEBCPP_XSTATE_MICROSTEP_HPP
