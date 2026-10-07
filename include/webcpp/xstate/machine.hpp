// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 A machine: the immutable tree of state nodes create_machine builds from
 XState's createMachine config, with the implementations it was given; and
 the lookup of a node by id or by path (StateMachine.ts, StateNode.ts,
 stateUtils.ts).

 Tip: a machine is a handle to data nothing changes once created, so copying
 one is cheap and a cursor may keep it.
*/
#ifndef WEBCPP_XSTATE_MACHINE_HPP
#define WEBCPP_XSTATE_MACHINE_HPP

#include <webcpp/xstate/errors.hpp>
#include <webcpp/xstate/implementations.hpp>
#include <webcpp/xstate/state_node.hpp>
#include <webcpp/xstate/state_value.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace webcpp::xstate {

namespace detail {

/**
 The number a key stands for when it is an array index, which JavaScript
 enumerates before every other key of an object, by number: "0", or digits
 without a leading zero, below 2^32 - 1.
*/
inline std::optional<std::uint64_t> array_index_of(std::string_view key) {
    constexpr std::uint64_t largest = 4294967294;
    const bool canonical =
        !key.empty() && key.size() <= 10 && (key.size() == 1 || key.front() != '0');
    if (!canonical || key.find_first_not_of("0123456789") != std::string_view::npos) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    const auto [end, failed] = std::from_chars(key.data(), key.data() + key.size(), value);
    if (failed != std::errc{} || value > largest) {
        return std::nullopt;
    }
    return value;
}

/**
 Whether JavaScript enumerates an object's key `left` before `right`, which
 was inserted after it: array indices first, by value, then the rest as
 inserted.
*/
inline bool enumerated_before(std::string_view left, std::string_view right) {
    const std::optional<std::uint64_t> left_index = array_index_of(left);
    const std::optional<std::uint64_t> right_index = array_index_of(right);
    if (left_index.has_value() && right_index.has_value()) {
        return *left_index < *right_index;
    }
    return left_index.has_value() && !right_index.has_value();
}

/**
 An object's members in the order JavaScript enumerates its keys, the order
 XState reads a config's `states`, `on` and `after` in.
*/
inline std::vector<const boost::json::key_value_pair*> enumerated(
    const boost::json::object& object) {
    std::vector<const boost::json::key_value_pair*> members;
    members.reserve(object.size());
    for (const boost::json::key_value_pair& member : object) {
        members.push_back(&member);
    }
    std::ranges::stable_sort(members, [](const auto* left, const auto* right) {
        return enumerated_before(left->key(), right->key());
    });
    return members;
}

struct machine_data {
    std::string id{};
    std::vector<state_node> nodes{};
    std::vector<guard_node> guards{};
    std::map<std::string, std::size_t, std::less<>> ids{};
    boost::json::value context = boost::json::object{};
    implementations registry{};
    boost::json::object config{};
};

/**
 The node a child key names under `from`; XState's getStateNode for a key
 that is not an id.
*/
inline result<std::size_t> child_of(const machine_data& data, std::size_t from,
                                    std::string_view key) {
    for (const auto& [child_key, child] : data.nodes[from].states) {
        if (child_key == key) {
            return child;
        }
    }
    return failure<std::size_t>(errc::unknown_state);
}

/**
 Walks `segments` from `from`, an empty segment ending the walk and a
 segment that starts with '#' restarting it at that id; XState's
 getStateNodeByPath and getStateNodeById, whose mutual recursion is the
 queue of segments still to walk.
*/
inline result<std::size_t> walk(const machine_data& data, std::size_t from,
                                std::deque<std::string> segments) {
    std::size_t current = from;
    while (!segments.empty()) {
        const std::string key = std::move(segments.front());
        segments.pop_front();
        if (key.empty()) {
            break;
        }
        if (key.front() != '#') {
            const result<std::size_t> child = child_of(data, current, key);
            if (!child.has_value()) {
                return child;
            }
            current = *child;
            continue;
        }
        std::vector<std::string> path = to_state_path(key);
        const auto found = data.ids.find(std::string_view(path.front()).substr(1));
        if (found == data.ids.end()) {
            return failure<std::size_t>(errc::unknown_state);
        }
        current = found->second;
        for (std::size_t index = path.size(); index > 1; --index) {
            segments.push_front(std::move(path[index - 1]));
        }
    }
    return current;
}

/**
 The node a state id names, '#' optional, a path after it relative to that
 node; XState's getStateNodeById.
*/
inline result<std::size_t> node_by_id(const machine_data& data, std::string_view state_id) {
    std::vector<std::string> path = to_state_path(state_id);
    std::string_view first = path.front();
    if (!first.empty() && first.front() == '#') {
        first.remove_prefix(1);
    }
    const auto found = data.ids.find(first);
    if (found == data.ids.end()) {
        return failure<std::size_t>(errc::unknown_state);
    }
    return walk(data, found->second, std::deque<std::string>(path.begin() + 1, path.end()));
}

/**
 The node a path names relative to `from`, or the node an id names when the
 path starts with '#'; XState's getStateNodeByPath.
*/
inline result<std::size_t> node_by_path(const machine_data& data, std::size_t from,
                                        std::string_view state_path) {
    if (!state_path.empty() && state_path.front() == '#') {
        return node_by_id(data, state_path);
    }
    const std::vector<std::string> path = to_state_path(state_path);
    return walk(data, from, std::deque<std::string>(path.begin(), path.end()));
}

/**
 A member of the config, or none when it is missing or null.

 Tip: XState reads most members so. A state's null `meta` it keeps, where
 xstate, reading it here too, has none (doc: #differences-null-meta).
*/
inline const boost::json::value* member(const boost::json::object& object, std::string_view key) {
    const boost::json::value* found = object.if_contains(key);
    return found == nullptr || found->is_null() ? nullptr : found;
}

/**
 A member XState reads with toArray, an action list, `tags` or a history's
 `target`: absent only when missing, since toArray makes a null a list that
 holds null.
*/
inline const boost::json::value* listed_member(const boost::json::object& object,
                                               std::string_view key) {
    return object.if_contains(key);
}

/**
 One action of a config: a name, or an object with a `type` and optional
 `params`.
*/
inline result<action_ref> parse_action(const boost::json::value& json) {
    if (json.is_string()) {
        return action_ref{.type = std::string(json.get_string()), .config = json};
    }
    if (!json.is_object()) {
        return failure<action_ref>(errc::invalid_config);
    }
    const boost::json::value* type = json.get_object().if_contains("type");
    if (type == nullptr || !type->is_string()) {
        return failure<action_ref>(errc::invalid_config);
    }
    const boost::json::value* params = member(json.get_object(), "params");
    return action_ref{
        .type = std::string(type->get_string()),
        .params = params == nullptr ? boost::json::value() : *params,
        .config = json,
    };
}

/** A config's action or list of actions; XState's toArray. */
inline result<std::vector<action_ref>> parse_actions(const boost::json::value* json) {
    std::vector<action_ref> actions;
    if (json == nullptr) {
        return actions;
    }
    if (!json->is_array()) {
        result<action_ref> one = parse_action(*json);
        if (!one.has_value()) {
            return one.error();
        }
        actions.push_back(std::move(*one));
        return actions;
    }
    for (const boost::json::value& item : json->get_array()) {
        result<action_ref> one = parse_action(item);
        if (!one.has_value()) {
            return one.error();
        }
        actions.push_back(std::move(*one));
    }
    return actions;
}

/**
 The kind of a higher-order guard's type, or nothing for any other type.
*/
inline std::optional<enum guard_node::kind> higher_order_kind(std::string_view type) {
    if (type == "xstate.not") {
        return guard_node::kind::negation;
    }
    if (type == "xstate.and") {
        return guard_node::kind::conjunction;
    }
    if (type == "xstate.or") {
        return guard_node::kind::disjunction;
    }
    return std::nullopt;
}

/**
 One guard's own fields from its config, a name or an object with a `type`;
 a higher-order guard's operands are left to the caller.
*/
inline result<guard_node> guard_of(const implementations& registry,
                                   const boost::json::value& config) {
    guard_node node{.config = config};
    if (config.is_string()) {
        node.type = std::string(config.get_string());
        if (!registry.guards.contains(node.type)) {
            return failure<guard_node>(errc::unknown_guard);
        }
        return node;
    }
    const boost::json::value* type =
        config.is_object() ? config.get_object().if_contains("type") : nullptr;
    if (type == nullptr || !type->is_string()) {
        return failure<guard_node>(errc::invalid_config);
    }
    const boost::json::object& object = config.get_object();
    node.type = std::string(type->get_string());
    if (const std::optional<enum guard_node::kind> kind = higher_order_kind(node.type)) {
        const boost::json::value* operands = object.if_contains("guards");
        const bool well_formed =
            operands != nullptr && operands->is_array() &&
            (*kind != guard_node::kind::negation || operands->get_array().size() == 1);
        if (!well_formed) {
            return failure<guard_node>(errc::invalid_config);
        }
        node.kind = *kind;
        return node;
    }
    if (node.type == "xstate.stateIn") {
        const boost::json::value* state_value = object.if_contains("stateValue");
        if (state_value == nullptr) {
            return failure<guard_node>(errc::invalid_config);
        }
        node.kind = guard_node::kind::state_in;
        node.state_value = *state_value;
        return node;
    }
    if (!registry.guards.contains(node.type)) {
        return failure<guard_node>(errc::unknown_guard);
    }
    const boost::json::value* params = member(object, "params");
    node.params = params == nullptr ? boost::json::value() : *params;
    return node;
}

/**
 Adds a config's guard and the guards it is made of to `guards`, and returns
 its index; a name not registered is unknown_guard.
*/
inline result<std::size_t> parse_guard(std::vector<guard_node>& guards,
                                       const implementations& registry,
                                       const boost::json::value& json) {
    const std::size_t root = guards.size();
    guards.emplace_back();
    std::vector<std::pair<std::size_t, const boost::json::value*>> pending{{root, &json}};
    while (!pending.empty()) {
        const auto [index, config] = pending.back();
        pending.pop_back();
        result<guard_node> read = guard_of(registry, *config);
        if (!read.has_value()) {
            return read.error();
        }
        guards[index] = std::move(*read);
        if (guards[index].kind != guard_node::kind::conjunction &&
            guards[index].kind != guard_node::kind::disjunction &&
            guards[index].kind != guard_node::kind::negation) {
            continue;
        }
        for (const boost::json::value& operand : config->get_object().at("guards").get_array()) {
            const std::size_t operand_index = guards.size();
            guards[index].operands.push_back(operand_index);
            guards.emplace_back();
            pending.emplace_back(operand_index, &operand);
        }
    }
    return root;
}

/**
 A config's transition or list of transitions as transition configs, a
 string or nothing being a target; XState's toTransitionConfigArray.
*/
inline result<std::vector<boost::json::object>> transition_configs(const boost::json::value& json) {
    std::vector<boost::json::object> configs;
    const auto add = [&configs](const boost::json::value& one) -> bool {
        if (one.is_null()) {
            configs.emplace_back();
            return true;
        }
        if (one.is_string()) {
            boost::json::object config;
            config.emplace("target", one);
            configs.push_back(std::move(config));
            return true;
        }
        if (!one.is_object()) {
            return false;
        }
        configs.push_back(one.get_object());
        return true;
    };
    if (json.is_array()) {
        for (const boost::json::value& one : json.get_array()) {
            if (!add(one)) {
                return failure<std::vector<boost::json::object>>(errc::invalid_config);
            }
        }
        return configs;
    }
    if (!add(json)) {
        return failure<std::vector<boost::json::object>>(errc::invalid_config);
    }
    return configs;
}

/**
 The node one target names from `source`; XState's resolveTarget for one
 target. An id names any node; a target that is not an id names a sibling,
 or, with a leading '.', a descendant of the source.

 Tip: an empty target names the source's parent, as XState's empty path
 does.
*/
inline result<std::size_t> resolve_target(const machine_data& data, std::size_t source,
                                          std::string_view target) {
    const state_node& node = data.nodes[source];
    if (!target.empty() && target.front() == '#') {
        return node_by_id(data, target);
    }
    const bool descendant = !target.empty() && target.front() == '.';
    if (!node.parent.has_value()) {
        if (!descendant) {
            return failure<std::size_t>(errc::unknown_target);
        }
        return node_by_path(data, source, target.substr(1));
    }
    const std::size_t parent = *node.parent;
    if (descendant) {
        return node_by_path(data, parent, node.key + std::string(target));
    }
    return node_by_path(data, parent, target);
}

/**
 The nodes a transition's targets name from `source`; XState's resolveTarget.
*/
inline result<std::vector<std::size_t>> resolve_targets(const machine_data& data,
                                                        std::size_t source,
                                                        const boost::json::value& targets) {
    std::vector<std::string_view> named;
    if (targets.is_string()) {
        named.emplace_back(targets.get_string());
    } else if (targets.is_array()) {
        for (const boost::json::value& one : targets.get_array()) {
            if (!one.is_string()) {
                return failure<std::vector<std::size_t>>(errc::invalid_config);
            }
            named.emplace_back(one.get_string());
        }
    } else {
        return failure<std::vector<std::size_t>>(errc::invalid_config);
    }
    std::vector<std::size_t> resolved;
    for (const std::string_view target : named) {
        const result<std::size_t> found = resolve_target(data, source, target);
        if (!found.has_value()) {
            return failure<std::vector<std::size_t>>(errc::unknown_target);
        }
        resolved.push_back(*found);
    }
    return resolved;
}

/** Whether JavaScript reads a JSON value as true: anything but false, 0, "" and null. */
inline bool truthy(const boost::json::value& value) {
    if (value.is_null()) {
        return false;
    }
    if (value.is_bool()) {
        return value.get_bool();
    }
    if (value.is_string()) {
        return !value.get_string().empty();
    }
    if (value.is_int64()) {
        return value.get_int64() != 0;
    }
    if (value.is_uint64()) {
        return value.get_uint64() != 0;
    }
    if (value.is_double()) {
        return value.get_double() != 0.0;
    }
    return true;
}

/**
 One transition of `source` from its config; XState's formatTransition.
*/
inline result<transition_definition> format_transition(machine_data& data, std::size_t source,
                                                       std::string_view descriptor,
                                                       boost::json::object config) {
    transition_definition formatted{.source = source, .event_type = std::string(descriptor)};
    const boost::json::value* target = member(config, "target");
    const bool targetless =
        target == nullptr || (target->is_string() && target->get_string().empty());
    if (!targetless) {
        result<std::vector<std::size_t>> resolved = resolve_targets(data, source, *target);
        if (!resolved.has_value()) {
            return resolved.error();
        }
        formatted.target = std::move(*resolved);
    }
    // XState's development build refuses `cond`, renamed `guard` in v5, when
    // it is truthy, as JavaScript reads it.
    if (const boost::json::value* cond = member(config, "cond"); cond != nullptr && truthy(*cond)) {
        return failure<transition_definition>(errc::invalid_config);
    }
    result<std::vector<action_ref>> actions = parse_actions(listed_member(config, "actions"));
    if (!actions.has_value()) {
        return actions.error();
    }
    formatted.actions = std::move(*actions);
    if (const boost::json::value* guard = member(config, "guard"); guard != nullptr) {
        const result<std::size_t> parsed = parse_guard(data.guards, data.registry, *guard);
        if (!parsed.has_value()) {
            return parsed.error();
        }
        formatted.guard = *parsed;
    }
    if (const boost::json::value* reenter = member(config, "reenter"); reenter != nullptr) {
        if (!reenter->is_bool()) {
            return failure<transition_definition>(errc::invalid_config);
        }
        formatted.reenter = reenter->get_bool();
    }
    formatted.config = std::move(config);
    return formatted;
}

/**
 Sets `added` under `descriptor`: a descriptor already there keeps its place
 in XState's Map and takes the new transitions, as Map.set does; XState's
 formatTransitions for `on`, `onDone` and an invoke's transitions.
*/
inline void set_transitions(state_node& node, std::string_view descriptor,
                            std::vector<transition_definition> added) {
    for (auto& [existing, transitions] : node.transitions) {
        if (existing == descriptor) {
            transitions = std::move(added);
            return;
        }
    }
    node.transitions.emplace_back(std::string(descriptor), std::move(added));
}

/**
 Appends `added` under `descriptor`, keeping XState's Map insertion order;
 XState's formatTransitions for the delayed transitions.
*/
inline void add_transitions(state_node& node, std::string_view descriptor,
                            std::vector<transition_definition> added) {
    for (auto& [existing, transitions] : node.transitions) {
        if (existing == descriptor) {
            for (transition_definition& one : added) {
                transitions.push_back(std::move(one));
            }
            return;
        }
    }
    node.transitions.emplace_back(std::string(descriptor), std::move(added));
}

/**
 Formats every transition in `json` under `descriptor`.
*/
inline result<std::vector<transition_definition>> format_all(machine_data& data, std::size_t source,
                                                             std::string_view descriptor,
                                                             const boost::json::value& json) {
    result<std::vector<boost::json::object>> configs = transition_configs(json);
    if (!configs.has_value()) {
        return configs.error();
    }
    std::vector<transition_definition> formatted;
    for (boost::json::object& config : *configs) {
        result<transition_definition> one =
            format_transition(data, source, descriptor, std::move(config));
        if (!one.has_value()) {
            return one.error();
        }
        formatted.push_back(std::move(*one));
    }
    return formatted;
}

/**
 An `after` key as a delay: a whole number of milliseconds, or the name of a
 registered delay.

 Tip: XState reads any numeric key as a number; a fraction or a negative
 number is refused here, as no delay of the clock can hold it.
*/
inline result<delay_ref> delay_of(const implementations& registry, std::string_view key) {
    const bool numeric =
        !key.empty() && key.find_first_not_of("0123456789") == std::string_view::npos;
    if (numeric) {
        std::uint64_t value = 0;
        const auto [end, failed] = std::from_chars(key.data(), key.data() + key.size(), value);
        if (failed != std::errc{} || end != key.data() + key.size()) {
            return failure<delay_ref>(errc::invalid_config);
        }
        return delay_ref{value};
    }
    if (!key.empty() &&
        std::string_view("0123456789.-+").find(key.front()) != std::string_view::npos) {
        return failure<delay_ref>(errc::invalid_config);
    }
    if (!registry.delays.contains(key)) {
        return failure<delay_ref>(errc::unknown_delay);
    }
    return delay_ref{std::string(key)};
}

/** The text a delay takes in an after event's type. */
inline std::string delay_text(const delay_ref& delay) {
    if (const std::uint64_t* milliseconds = std::get_if<std::uint64_t>(&delay)) {
        return std::to_string(*milliseconds);
    }
    return std::get<std::string>(delay);
}

/** Formats a node's `on`; XState's formatTransitions, for `on`. */
inline result<void> initialize_on(machine_data& data, std::size_t index,
                                  const boost::json::value& on) {
    if (!on.is_object()) {
        return failure<void>(errc::invalid_config);
    }
    for (const boost::json::key_value_pair* entry : enumerated(on.get_object())) {
        if (entry->key().empty()) {
            return failure<void>(errc::invalid_config);
        }
        result<std::vector<transition_definition>> formatted =
            format_all(data, index, entry->key(), entry->value());
        if (!formatted.has_value()) {
            return formatted.error();
        }
        set_transitions(data.nodes[index], entry->key(), std::move(*formatted));
    }
    return {};
}

/** Formats a node's `onDone` under xstate.done.state.<id>. */
inline result<void> initialize_on_done(machine_data& data, std::size_t index,
                                       const boost::json::value& on_done) {
    const std::string descriptor = "xstate.done.state." + data.nodes[index].id;
    result<std::vector<transition_definition>> formatted =
        format_all(data, index, descriptor, on_done);
    if (!formatted.has_value()) {
        return formatted.error();
    }
    set_transitions(data.nodes[index], descriptor, std::move(*formatted));
    return {};
}

/**
 Formats each invoke's `onDone`, `onError` and `onSnapshot` under the events
 its actor sends its parent; XState's formatTransitions, for invoke.
*/
inline result<void> initialize_invoke(machine_data& data, std::size_t index) {
    constexpr std::array<std::pair<std::string_view, std::string_view>, 3> handlers{
        {
            {"onDone", "xstate.done.actor."},
            {"onError", "xstate.error.actor."},
            {"onSnapshot", "xstate.snapshot."},
        },
    };
    const std::vector<invoke_definition> invokes = data.nodes[index].invoke;
    for (const invoke_definition& invoked : invokes) {
        for (const auto& [key, prefix] : handlers) {
            const boost::json::value* handler = member(invoked.config, key);
            if (handler == nullptr) {
                continue;
            }
            const std::string descriptor = std::string(prefix) + invoked.id;
            result<std::vector<transition_definition>> formatted =
                format_all(data, index, descriptor, *handler);
            if (!formatted.has_value()) {
                return formatted.error();
            }
            set_transitions(data.nodes[index], descriptor, std::move(*formatted));
        }
    }
    return {};
}

/**
 Formats a node's `after`: each delay raises its after event on entry and
 cancels it on exit, and its transitions take that event; XState's
 getDelayedTransitions.
*/
inline result<void> initialize_after(machine_data& data, std::size_t index,
                                     const boost::json::value& after) {
    if (!after.is_object()) {
        return failure<void>(errc::invalid_config);
    }
    for (const boost::json::key_value_pair* entry : enumerated(after.get_object())) {
        const result<delay_ref> delay = delay_of(data.registry, entry->key());
        if (!delay.has_value()) {
            return delay.error();
        }
        const std::string event_type =
            "xstate.after." + delay_text(*delay) + "." + data.nodes[index].id;
        data.nodes[index].entry.push_back(action_ref{
            .kind = action_ref::kind::after_raise,
            .type = event_type,
            .delay = *delay,
        });
        data.nodes[index].exit.push_back(
            action_ref{.kind = action_ref::kind::after_cancel, .type = event_type});
        result<std::vector<transition_definition>> formatted =
            format_all(data, index, event_type, entry->value());
        if (!formatted.has_value()) {
            return formatted.error();
        }
        for (transition_definition& delayed : *formatted) {
            delayed.delay = *delay;
        }
        add_transitions(data.nodes[index], event_type, std::move(*formatted));
    }
    return {};
}

/**
 Formats a node's `on`, `onDone`, invokes, `after` and `always`, in that
 order, the order of XState's transition Map; XState's StateNode
 _initialize.
*/
inline result<void> initialize(machine_data& data, std::size_t index) {
    const boost::json::object config = data.nodes[index].config;
    if (const boost::json::value* on = member(config, "on"); on != nullptr) {
        if (const result<void> done = initialize_on(data, index, *on); !done.has_value()) {
            return done;
        }
    }
    if (const boost::json::value* on_done = member(config, "onDone"); on_done != nullptr) {
        if (const result<void> done = initialize_on_done(data, index, *on_done);
            !done.has_value()) {
            return done;
        }
    }
    if (const result<void> done = initialize_invoke(data, index); !done.has_value()) {
        return done;
    }
    if (const boost::json::value* after = member(config, "after"); after != nullptr) {
        if (const result<void> done = initialize_after(data, index, *after); !done.has_value()) {
            return done;
        }
    }
    if (const boost::json::value* always = member(config, "always"); always != nullptr) {
        result<std::vector<transition_definition>> formatted = format_all(data, index, "", *always);
        if (!formatted.has_value()) {
            return formatted.error();
        }
        data.nodes[index].always = std::move(*formatted);
    }
    return {};
}

/**
 A node's initial transition: to the child `initial` names, with the
 actions an object form gives; XState's formatInitialTransition.
*/
inline result<void> initial_of(machine_data& data, std::size_t index) {
    state_node& node = data.nodes[index];
    node.initial = transition_definition{.source = index, .target = std::vector<std::size_t>{}};
    const boost::json::value* initial = member(node.config, "initial");
    if (initial == nullptr) {
        if (node.type == node_type::compound) {
            return failure<void>(errc::invalid_config);
        }
        return {};
    }
    const boost::json::value* key = initial;
    if (initial->is_object()) {
        key = member(initial->get_object(), "target");
        result<std::vector<action_ref>> actions =
            parse_actions(listed_member(initial->get_object(), "actions"));
        if (!actions.has_value()) {
            return actions.error();
        }
        node.initial.actions = std::move(*actions);
        node.initial.config = initial->get_object();
    }
    if (key == nullptr || !key->is_string()) {
        return failure<void>(errc::invalid_config);
    }
    const result<std::size_t> child = child_of(data, index, key->get_string());
    if (!child.has_value()) {
        return failure<void>(errc::unknown_target);
    }
    node.initial.target = std::vector<std::size_t>{*child};
    return {};
}

/**
 A history node's default target, each relative to its parent; XState's
 resolveHistoryDefaultTransition, for a node that names one.
*/
inline result<void> history_target_of(machine_data& data, std::size_t index) {
    state_node& node = data.nodes[index];
    // XState's normalizeTarget makes a null target a list that holds null.
    const boost::json::value* target = listed_member(node.config, "target");
    if (node.type != node_type::history || target == nullptr ||
        (target->is_string() && target->get_string().empty())) {
        return {};
    }
    // A history at the root has no parent its target could name a child of:
    // XState accepts the machine and throws where it needs the default,
    // unless the target is an empty list, which names nothing.
    if (!node.parent.has_value()) {
        const bool strings =
            target->is_string() ||
            (target->is_array() &&
             std::ranges::all_of(target->get_array(),
                                 [](const boost::json::value& one) { return one.is_string(); }));
        if (!strings) {
            return failure<void>(errc::invalid_config);
        }
        if (target->is_array() && target->get_array().empty()) {
            node.history_target = std::vector<std::size_t>{};
        }
        return {};
    }
    std::vector<std::size_t> resolved;
    const auto add = [&](const boost::json::value& one) -> result<void> {
        if (!one.is_string()) {
            return failure<void>(errc::invalid_config);
        }
        const result<std::size_t> found = node_by_path(data, *node.parent, one.get_string());
        if (!found.has_value()) {
            return failure<void>(errc::unknown_target);
        }
        resolved.push_back(*found);
        return {};
    };
    if (target->is_array()) {
        for (const boost::json::value& one : target->get_array()) {
            if (const result<void> added = add(one); !added.has_value()) {
                return added;
            }
        }
    } else if (const result<void> added = add(*target); !added.has_value()) {
        return added;
    }
    data.nodes[index].history_target = std::move(resolved);
    return {};
}

/**
 A node's type: the config's `type`, or compound for a node with children,
 history for one whose `history` is truthy, atomic otherwise; XState's
 StateNode constructor, which reads `history` as JavaScript does.
*/
inline result<node_type> type_of(const boost::json::object& config) {
    const boost::json::value* type = member(config, "type");
    if (type == nullptr) {
        const boost::json::value* states = member(config, "states");
        const boost::json::value* history = member(config, "history");
        if (states != nullptr && states->is_object() && !states->get_object().empty()) {
            return node_type::compound;
        }
        if (history != nullptr && truthy(*history)) {
            return node_type::history;
        }
        return node_type::atomic;
    }
    if (!type->is_string()) {
        return failure<node_type>(errc::invalid_config);
    }
    const std::string_view named = type->get_string();
    constexpr std::array<std::pair<std::string_view, node_type>, 5> types{
        {
            {"atomic", node_type::atomic},
            {"compound", node_type::compound},
            {"parallel", node_type::parallel},
            {"final", node_type::final},
            {"history", node_type::history},
        },
    };
    for (const auto& [spelled, value] : types) {
        if (named == spelled) {
            return value;
        }
    }
    return failure<node_type>(errc::invalid_config);
}

/**
 A node's `history`: "deep" is deep, any other truthy value shallow, as
 XState resolves every history but 'deep' as shallow.
*/
inline history_kind history_of(const boost::json::object& config) {
    const boost::json::value* history = member(config, "history");
    if (history == nullptr || !truthy(*history)) {
        return history_kind::none;
    }
    if (history->is_string() && history->get_string() == "deep") {
        return history_kind::deep;
    }
    return history_kind::shallow;
}

/** A node's `tags`, one or a list of strings. */
inline result<std::vector<std::string>> tags_of(const boost::json::object& config) {
    std::vector<std::string> tags;
    const boost::json::value* given = listed_member(config, "tags");
    if (given == nullptr) {
        return tags;
    }
    const boost::json::array one{*given};
    const boost::json::array& listed = given->is_array() ? given->get_array() : one;
    for (const boost::json::value& tag : listed) {
        if (!tag.is_string()) {
            return failure<std::vector<std::string>>(errc::invalid_config);
        }
        tags.emplace_back(tag.get_string());
    }
    return tags;
}

/**
 A history node's default targets: its own, its parallel parent, or its
 parent's initial ones; none for a history at the root, whose default
 fails where it is needed instead of leading anywhere.
*/
inline std::vector<std::size_t> history_defaults(const machine_data& data, std::size_t node) {
    const state_node& history = data.nodes[node];
    if (history.history_target.has_value()) {
        return *history.history_target;
    }
    if (!history.parent.has_value()) {
        return {};
    }
    const state_node& parent = data.nodes[*history.parent];
    if (parent.type == node_type::parallel) {
        return {*history.parent};
    }
    return parent.initial.target.value_or(std::vector<std::size_t>{});
}

/**
 The nodes entering `node` by default enters next: a compound's initial
 child, a parallel's regions, a history node's default targets, as nothing
 is remembered the first time it is entered.
*/
inline std::vector<std::size_t> default_entries(const machine_data& data, std::size_t node) {
    const state_node& entered = data.nodes[node];
    if (entered.type == node_type::history) {
        return history_defaults(data, node);
    }
    if (entered.type == node_type::compound) {
        return entered.initial.target.value_or(std::vector<std::size_t>{});
    }
    std::vector<std::size_t> regions;
    if (entered.type == node_type::parallel) {
        for (const auto& [key, child] : entered.states) {
            if (data.nodes[child].type != node_type::history) {
                regions.push_back(child);
            }
        }
    }
    return regions;
}

/**
 Refuses a machine where entering a node by default leads back to it, as a
 history default target that is an ancestor whose initial chain reaches the
 history again: entering it would never end, where XState overflows its
 stack. A depth-first walk of the default entries, marking the nodes on the
 current path.
*/
inline result<void> check_default_entries(const machine_data& data) {
    enum class mark : unsigned char { unvisited, on_path, done };
    std::vector<mark> marks(data.nodes.size(), mark::unvisited);
    for (std::size_t start = 0; start < data.nodes.size(); ++start) {
        if (marks[start] != mark::unvisited) {
            continue;
        }
        std::vector<std::pair<std::size_t, std::vector<std::size_t>>> path;
        marks[start] = mark::on_path;
        path.emplace_back(start, default_entries(data, start));
        while (!path.empty()) {
            std::vector<std::size_t>& next = path.back().second;
            if (next.empty()) {
                marks[path.back().first] = mark::done;
                path.pop_back();
                continue;
            }
            const std::size_t child = next.back();
            next.pop_back();
            if (marks[child] == mark::on_path) {
                return failure<void>(errc::invalid_config);
            }
            if (marks[child] == mark::unvisited) {
                marks[child] = mark::on_path;
                path.emplace_back(child, default_entries(data, child));
            }
        }
    }
    return {};
}

/**
 A node's `invoke`, one object or a list, each naming its actor by a string
 `src`; XState's StateNode invoke, whose default id is createInvokeId.

 Tip: XState also takes inline logic as `src`, which JSON cannot hold.
*/
inline result<std::vector<invoke_definition>> invokes_of(const boost::json::object& config,
                                                         std::string_view node_id) {
    std::vector<invoke_definition> invokes;
    const boost::json::value* given = member(config, "invoke");
    if (given == nullptr) {
        return invokes;
    }
    std::vector<const boost::json::value*> listed;
    if (given->is_array()) {
        for (const boost::json::value& one : given->get_array()) {
            listed.push_back(&one);
        }
    } else {
        listed.push_back(given);
    }
    for (const boost::json::value* one : listed) {
        const boost::json::object* invoke_config = one->if_object();
        const boost::json::value* src =
            invoke_config == nullptr ? nullptr : member(*invoke_config, "src");
        if (src == nullptr || !src->is_string()) {
            return failure<std::vector<invoke_definition>>(errc::invalid_config);
        }
        const boost::json::value* id = member(*invoke_config, "id");
        const boost::json::value* system_id = member(*invoke_config, "systemId");
        if ((id != nullptr && !id->is_string()) ||
            (system_id != nullptr && !system_id->is_string())) {
            return failure<std::vector<invoke_definition>>(errc::invalid_config);
        }
        invoke_definition made{
            .id = id != nullptr ? std::string(id->get_string())
                                : std::to_string(invokes.size()) + "." + std::string(node_id),
            .src = std::string(src->get_string()),
            .config = *invoke_config,
        };
        if (const boost::json::value* input = invoke_config->if_contains("input")) {
            made.input = *input;
        }
        if (system_id != nullptr) {
            made.system_id = std::string(system_id->get_string());
        }
        invokes.push_back(std::move(made));
    }
    return invokes;
}

/**
 The node a config describes, under `parent` with `key`; XState's StateNode
 constructor, without its children.
*/
inline result<state_node> node_of(const machine_data& data, const boost::json::object& config,
                                  std::optional<std::size_t> parent, std::string key) {
    const boost::json::value* id = member(config, "id");
    const result<node_type> type = type_of(config);
    result<std::vector<action_ref>> entry = parse_actions(listed_member(config, "entry"));
    result<std::vector<action_ref>> exit = parse_actions(listed_member(config, "exit"));
    result<std::vector<std::string>> tags = tags_of(config);
    if ((id != nullptr && !id->is_string()) || !type.has_value() || !entry.has_value() ||
        !exit.has_value() || !tags.has_value()) {
        return failure<state_node>(errc::invalid_config);
    }
    state_node node{
        .key = std::move(key),
        .parent = parent,
        .type = *type,
        .history = history_of(config),
        .entry = std::move(*entry),
        .exit = std::move(*exit),
        .tags = std::move(*tags),
        .config = config,
    };
    // The children are nodes of their own; a node keeps only its own keys.
    node.config.erase("states");
    if (parent.has_value()) {
        node.path = data.nodes[*parent].path;
        node.path.push_back(node.key);
    }
    if (id != nullptr) {
        node.id = std::string(id->get_string());
    } else {
        node.id = data.id;
        for (const std::string& segment : node.path) {
            node.id += "." + segment;
        }
    }
    result<std::vector<invoke_definition>> invokes = invokes_of(config, node.id);
    if (!invokes.has_value()) {
        return invokes.error();
    }
    node.invoke = std::move(*invokes);
    if (const boost::json::value* meta = member(config, "meta"); meta != nullptr) {
        node.meta = *meta;
    }
    if (const boost::json::value* description = member(config, "description");
        description != nullptr && description->is_string()) {
        node.description = std::string(description->get_string());
    }
    const boost::json::value* output = config.if_contains("output");
    if (output != nullptr && (node.type == node_type::final || !parent.has_value())) {
        node.output = *output;
    }
    return node;
}

/**
 Builds every node of the config, in document order: a parent before its
 children, and the children in the order JavaScript enumerates the keys
 of `states`, which is XState's `order`.
*/
inline result<void> build_nodes(machine_data& data) {
    struct pending_node {
        const boost::json::object* config{};
        std::optional<std::size_t> parent{};
        std::string key{};
    };

    std::vector<pending_node> pending{
        {.config = &data.config, .parent = std::nullopt, .key = data.id},
    };
    while (!pending.empty()) {
        pending_node next = std::move(pending.back());
        pending.pop_back();
        result<state_node> built = node_of(data, *next.config, next.parent, std::move(next.key));
        if (!built.has_value()) {
            return built.error();
        }
        const std::size_t index = data.nodes.size();
        built->order = data.ids.size();
        data.ids.insert_or_assign(built->id, index);
        data.nodes.push_back(std::move(*built));
        if (next.parent.has_value()) {
            data.nodes[*next.parent].states.emplace_back(data.nodes[index].key, index);
        }
        const boost::json::value* states = member(*next.config, "states");
        if (states == nullptr) {
            continue;
        }
        if (!states->is_object()) {
            return failure<void>(errc::invalid_config);
        }
        // Pushed last to first, so the first child is built first.
        const std::vector<const boost::json::key_value_pair*> children =
            enumerated(states->get_object());
        for (const boost::json::key_value_pair* child : std::ranges::reverse_view(children)) {
            if (!child->value().is_object()) {
                return failure<void>(errc::invalid_config);
            }
            pending.push_back({
                .config = &child->value().get_object(),
                .parent = index,
                .key = std::string(child->key()),
            });
        }
    }
    return {};
}

}  // namespace detail

class machine {
public:
    [[nodiscard]] const std::string& id() const noexcept { return data_->id; }

    [[nodiscard]] std::size_t size() const noexcept { return data_->nodes.size(); }

    [[nodiscard]] const state_node& node(std::size_t index) const { return data_->nodes.at(index); }

    [[nodiscard]] const state_node& root() const { return data_->nodes.front(); }

    [[nodiscard]] const guard_node& guard(std::size_t index) const {
        return data_->guards.at(index);
    }

    [[nodiscard]] const boost::json::value& context() const noexcept { return data_->context; }

    [[nodiscard]] const implementations& registry() const noexcept { return data_->registry; }

    [[nodiscard]] const boost::json::object& config() const noexcept { return data_->config; }

    /** The node a state id names; XState's getStateNodeById. */
    [[nodiscard]] result<std::size_t> node_by_id(std::string_view state_id) const {
        return detail::node_by_id(*data_, state_id);
    }

    /** The child of `node` whose key is `key`, a dot being part of the key. */
    [[nodiscard]] result<std::size_t> child(std::size_t node, std::string_view key) const {
        if (node >= data_->nodes.size()) {
            return failure<std::size_t>(errc::unknown_state);
        }
        return detail::child_of(*data_, node, key);
    }

    /** The node a path names relative to `from`; XState's getStateNodeByPath. */
    [[nodiscard]] result<std::size_t> node_by_path(std::size_t from,
                                                   std::string_view state_path) const {
        if (from >= data_->nodes.size()) {
            return failure<std::size_t>(errc::unknown_state);
        }
        return detail::node_by_path(*data_, from, state_path);
    }

private:
    explicit machine(std::shared_ptr<const detail::machine_data> data) noexcept
        : data_(std::move(data)) {}

    friend result<machine> create_machine(const boost::json::value& config,
                                          implementations registry);

    std::shared_ptr<const detail::machine_data> data_;
};

inline machine_actor::machine_actor(machine run)
    : logic(std::make_shared<const machine>(std::move(run))) {}

namespace detail {

/**
 Whether an implementation holds an empty function, which calling would
 abort without exceptions; an optional one left out holds none.
*/
inline bool holds_empty_function(const implementations& registry) {
    const auto empty_action = [](const auto& named) {
        return std::visit(
            [](const auto& one) {
                using kind = std::decay_t<decltype(one)>;
                if constexpr (std::is_same_v<kind, assign_action>) {
                    return !one.assignment;
                } else if constexpr (std::is_same_v<kind, log_action>) {
                    return one.value.has_value() && !*one.value;
                } else if constexpr (std::is_same_v<kind, spawn_child_action>) {
                    const auto* computed_id = std::get_if<id_maker>(&one.id);
                    return (one.input.has_value() && !*one.input) ||
                           (computed_id != nullptr && !*computed_id);
                } else if constexpr (requires { one.event; }) {
                    return !one.event;
                } else {
                    return false;
                }
            },
            named.second);
    };
    const auto empty = [](const auto& one) { return !one.second; };
    return (registry.context.has_value() && !*registry.context) ||
           std::ranges::any_of(registry.actions, empty_action) ||
           std::ranges::any_of(registry.guards, empty) ||
           std::ranges::any_of(registry.delays, empty) ||
           std::ranges::any_of(registry.inputs, empty) ||
           std::ranges::any_of(registry.outputs, empty);
}

/** Whether every computed input names an invoke that has no input in the config. */
inline bool inputs_name_invokes(const machine_data& data) {
    return std::ranges::all_of(data.registry.inputs, [&data](const auto& computed) {
        bool named = false;
        for (const state_node& node : data.nodes) {
            for (const invoke_definition& invoked : node.invoke) {
                if (invoked.id != computed.first) {
                    continue;
                }
                if (invoked.input.has_value()) {
                    return false;
                }
                named = true;
            }
        }
        return named;
    });
}

/**
 Whether every computed output names a final state, or the root, that has
 no output in the config.
*/
inline bool outputs_name_final_states(const machine_data& data) {
    return std::ranges::all_of(data.registry.outputs, [&data](const auto& computed) {
        const auto named = std::ranges::find_if(
            data.nodes, [&computed](const state_node& node) { return node.id == computed.first; });
        return named != data.nodes.end() && !named->output.has_value() &&
               (named == data.nodes.begin() || named->type == node_type::final);
    });
}

/**
 Whether every actor the machine names, an invoke's src or the src of a
 spawnChild the config names as an action, is one of the implementations'
 actors.

 Tip: XState creates no child for an unknown src and fails later, when
 something reads it; xstate refuses it with the machine. A registered
 spawnChild the config never names is not the machine's, and XState's
 setup accepts it.
*/
inline bool names_known_actors(const machine_data& data) {
    const auto known_spawn = [&data](const action_ref& named) {
        const auto registered = data.registry.actions.find(named.type);
        if (named.kind != action_ref::kind::named || registered == data.registry.actions.end()) {
            return true;
        }
        const auto* spawned = std::get_if<spawn_child_action>(&registered->second);
        return spawned == nullptr || data.registry.actors.contains(spawned->src);
    };
    const auto actions_known = [&known_spawn](const std::vector<action_ref>& actions) {
        return std::ranges::all_of(actions, known_spawn);
    };
    const auto transitions_known = [&actions_known](
                                       const std::vector<transition_definition>& transitions) {
        return std::ranges::all_of(transitions, [&actions_known](const transition_definition& one) {
            return actions_known(one.actions);
        });
    };
    return std::ranges::all_of(data.nodes, [&](const state_node& node) {
        const bool invokes_known =
            std::ranges::all_of(node.invoke, [&data](const invoke_definition& invoked) {
                return data.registry.actors.contains(invoked.src);
            });
        const bool described_known =
            std::ranges::all_of(node.transitions, [&transitions_known](const auto& described) {
                return transitions_known(described.second);
            });
        return invokes_known && described_known && actions_known(node.entry) &&
               actions_known(node.exit) && actions_known(node.initial.actions) &&
               transitions_known(node.always);
    });
}

}  // namespace detail

/**
 Builds a machine from XState's createMachine config and the implementations
 it names; XState's StateMachine constructor. Refuses a config XState would
 refuse, an implementation holding an empty function, a guard or a delay the
 registry does not hold, and a target that names no state
 (doc: #xstate-invariant-1).

 Tip: nodes are built in document order, a parent before its children,
 and the children in the order JavaScript enumerates the keys of `states`,
 which is XState's `order`; transitions are formatted once every node
 exists, because a target may name any id.
*/
inline result<machine> create_machine(const boost::json::value& config, implementations registry) {
    if (!config.is_object() || detail::holds_empty_function(registry)) {
        return failure<machine>(errc::invalid_config);
    }
    auto data = std::make_shared<detail::machine_data>();
    data->config = config.get_object();
    data->registry = std::move(registry);
    const boost::json::value* id = detail::member(data->config, "id");
    if (id != nullptr && !id->is_string()) {
        return failure<machine>(errc::invalid_config);
    }
    data->id = id == nullptr ? "(machine)" : std::string(id->get_string());
    if (const boost::json::value* context = detail::member(data->config, "context");
        context != nullptr) {
        data->context = *context;
    }
    if (const result<void> built = detail::build_nodes(*data); !built.has_value()) {
        return built.error();
    }
    for (std::size_t index = 0; index < data->nodes.size(); ++index) {
        if (const result<void> initial = detail::initial_of(*data, index); !initial.has_value()) {
            return initial.error();
        }
    }
    for (std::size_t index = 0; index < data->nodes.size(); ++index) {
        const result<void> formatted = detail::initialize(*data, index);
        const result<void> history =
            formatted.has_value() ? detail::history_target_of(*data, index) : formatted;
        if (!history.has_value()) {
            return history.error();
        }
    }
    if (const result<void> checked = detail::check_default_entries(*data); !checked.has_value()) {
        return checked.error();
    }
    if (!detail::names_known_actors(*data)) {
        return failure<machine>(errc::unknown_actor);
    }
    if (!detail::inputs_name_invokes(*data) || !detail::outputs_name_final_states(*data)) {
        return failure<machine>(errc::invalid_config);
    }
    return machine(std::move(data));
}

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_MACHINE_HPP
