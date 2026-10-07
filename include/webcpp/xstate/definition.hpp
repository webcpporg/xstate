// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 A machine's definition, as XState's machine.toJSON() writes it, and a
 machine loaded back from one (StateNode.ts definition, toSerializableAction;
 stateUtils.ts formatTransition's toJSON).

 Tip: XState's definition leaves out what JSON cannot hold or what it does
 not list: eventless transitions, the context, and the event and delay of
 the raise an `after` adds. A machine loaded from a definition is given its
 context, and takes the original's steps when the original has no `always`
 and no higher-order guard (doc: #xstate-invariant-14).
*/
#ifndef WEBCPP_XSTATE_DEFINITION_HPP
#define WEBCPP_XSTATE_DEFINITION_HPP

#include <webcpp/xstate/errors.hpp>
#include <webcpp/xstate/implementations.hpp>
#include <webcpp/xstate/machine.hpp>
#include <webcpp/xstate/state_node.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace webcpp::xstate {

namespace detail {

/** An action as XState serialises it: a name becomes {type}; XState's toSerializableAction. */
inline boost::json::value serialized_action(const action_ref& action) {
    if (action.kind == action_ref::kind::after_raise) {
        return boost::json::object{{"type", "xstate.raise"}};
    }
    if (action.kind == action_ref::kind::after_cancel) {
        return boost::json::object{{"type", "xstate.cancel"}};
    }
    if (action.config.is_string()) {
        return boost::json::object{{"type", action.config}};
    }
    return action.config;
}

/** A list of node ids as XState writes targets: "#id" each. */
inline boost::json::array targets_json(const machine& owner,
                                       const std::vector<std::size_t>& nodes) {
    boost::json::array targets;
    for (const std::size_t node : nodes) {
        targets.emplace_back("#" + owner.node(node).id);
    }
    return targets;
}

/**
 A transition as XState writes it: its config, with the actions as the
 config listed them, its targets as ids, its source, whether it reenters
 and its event; formatTransition's toJSON.
*/
inline boost::json::object transition_json(const machine& owner, const transition_definition& one) {
    boost::json::object json = one.config;
    if (!json.contains("actions")) {
        json["actions"] = boost::json::array{};
    } else if (!json.at("actions").is_array()) {
        json["actions"] = boost::json::array{json.at("actions")};
    }
    json.erase("target");
    if (one.target.has_value()) {
        json["target"] = targets_json(owner, *one.target);
    }
    if (one.delay.has_value()) {
        json["event"] = one.event_type;
        if (const std::uint64_t* milliseconds = std::get_if<std::uint64_t>(&*one.delay)) {
            json["delay"] = *milliseconds;
        } else {
            json["delay"] = std::get<std::string>(*one.delay);
        }
    }
    json["source"] = "#" + owner.node(one.source).id;
    json["reenter"] = one.reenter;
    json["eventType"] = one.event_type;
    return json;
}

/** A node's initial transition as XState writes it. */
inline boost::json::object initial_json(const machine& owner, const state_node& node) {
    boost::json::array actions;
    for (const action_ref& action : node.initial.actions) {
        actions.push_back(serialized_action(action));
    }
    boost::json::object json{
        {"target", targets_json(owner, node.initial.target.value_or(std::vector<std::size_t>{}))},
        {"source", "#" + node.id},
        {"actions", std::move(actions)},
        {"eventType", nullptr},
    };
    for (const char* key : {"meta", "description"}) {
        if (const boost::json::value* value = node.initial.config.if_contains(key)) {
            json[key] = *value;
        }
    }
    return json;
}

/** A node type as XState spells it. */
inline const char* spelled(node_type type) noexcept {
    switch (type) {
        case node_type::atomic: return "atomic";
        case node_type::compound: return "compound";
        case node_type::parallel: return "parallel";
        case node_type::final: return "final";
        case node_type::history: return "history";
    }
    return "atomic";
}

/**
 An invoke as XState's definition writes it: its config without `onDone`
 and `onError`, which the node's transitions hold, with its type, src and
 id; XState's InvokeDefinition toJSON.
*/
inline boost::json::object invoke_json(const invoke_definition& invoked) {
    boost::json::object json = invoked.config;
    json.erase("onDone");
    json.erase("onError");
    json["type"] = "xstate.invoke";
    json["src"] = invoked.src;
    json["id"] = invoked.id;
    return json;
}

/** A node's own definition, its children left to the caller. */
inline boost::json::object node_json(const machine& owner, std::size_t index) {
    const state_node& node = owner.node(index);
    boost::json::object json{
        {"id", node.id},
        {"key", node.key},
        {"type", spelled(node.type)},
        {"initial", initial_json(owner, node)},
        {"states", boost::json::object{}},
    };
    // XState writes true as "shallow", a falsy history as false, and any
    // other value as the config gave it.
    const boost::json::value* history = node.config.if_contains("history");
    if (history == nullptr || !truthy(*history)) {
        json["history"] = false;
    } else if (history->is_bool()) {
        json["history"] = "shallow";
    } else {
        json["history"] = *history;
    }
    boost::json::object on;
    boost::json::array transitions;
    for (const auto& [descriptor, listed] : node.transitions) {
        boost::json::array under;
        for (const transition_definition& one : listed) {
            under.emplace_back(transition_json(owner, one));
            transitions.emplace_back(transition_json(owner, one));
        }
        on[descriptor] = std::move(under);
    }
    json["on"] = std::move(on);
    json["transitions"] = std::move(transitions);
    boost::json::array entry;
    for (const action_ref& action : node.entry) {
        entry.push_back(serialized_action(action));
    }
    boost::json::array exit;
    for (const action_ref& action : node.exit) {
        exit.push_back(serialized_action(action));
    }
    json["entry"] = std::move(entry);
    json["exit"] = std::move(exit);
    if (node.meta.has_value()) {
        json["meta"] = *node.meta;
    }
    // XState writes `this.order || -1`, so the root's order 0 is written -1.
    json["order"] = node.order == 0 ? std::int64_t{-1} : static_cast<std::int64_t>(node.order);
    if (node.output.has_value()) {
        json["output"] = *node.output;
    }
    boost::json::array invoke;
    for (const invoke_definition& invoked : node.invoke) {
        invoke.emplace_back(invoke_json(invoked));
    }
    json["invoke"] = std::move(invoke);
    if (node.description.has_value()) {
        json["description"] = *node.description;
    }
    boost::json::array tags;
    for (const std::string& tag : node.tags) {
        tags.emplace_back(tag);
    }
    json["tags"] = std::move(tags);
    if (const boost::json::value* version = owner.config().if_contains("version")) {
        json["version"] = *version;
    }
    return json;
}

}  // namespace detail

/**
 A machine's definition, as XState's machine.toJSON() writes it; built
 deepest first, so each node's children are written before it.
*/
inline boost::json::value to_json(const machine& owner) {
    std::vector<boost::json::object> definitions(owner.size());
    for (std::size_t index = owner.size(); index > 0; --index) {
        const std::size_t node = index - 1;
        definitions[node] = detail::node_json(owner, node);
        boost::json::object& states = definitions[node]["states"].as_object();
        for (const auto& [key, child] : owner.node(node).states) {
            states[key] = std::move(definitions[child]);
        }
    }
    return definitions.front();
}

namespace detail {

/** The keys of a written transition that XState adds and a config does not have. */
inline bool is_written_only(std::string_view key) {
    return key == "source" || key == "eventType" || key == "event" || key == "delay";
}

/**
 A written target, "#" and a node's exact id, as a target that names that
 id alone: a dot or a backslash in the id escaped, so it is not read as a
 path.
*/
inline std::string exact_target(std::string_view written) {
    std::string target = "#";
    for (const char character : written.substr(1)) {
        if (character == '.' || character == '\\') {
            target += '\\';
        }
        target += character;
    }
    return target;
}

/** A written transition as the config that writes it. */
inline boost::json::object transition_config(const boost::json::object& written) {
    boost::json::object config;
    for (const auto& member : written) {
        if (!is_written_only(member.key())) {
            config.emplace(member.key(), member.value());
        }
    }
    if (boost::json::value* target = config.if_contains("target");
        target != nullptr && target->is_array()) {
        for (boost::json::value& one : target->get_array()) {
            if (one.is_string() && one.get_string().starts_with('#')) {
                one = exact_target(one.get_string());
            }
        }
    }
    return config;
}

/**
 A written entry or exit list without the raise and cancel `after` adds;
 invalid_config for one that is not a list, which XState never writes.
*/
inline result<boost::json::array> actions_config(const boost::json::value* written) {
    boost::json::array config;
    if (written == nullptr) {
        return config;
    }
    if (!written->is_array()) {
        return failure<boost::json::array>(errc::invalid_config);
    }
    for (const boost::json::value& action : written->get_array()) {
        const boost::json::value* type =
            action.is_object() ? action.get_object().if_contains("type") : nullptr;
        const bool generated =
            type != nullptr && type->is_string() &&
            (type->get_string() == "xstate.raise" || type->get_string() == "xstate.cancel");
        if (!generated) {
            config.push_back(action);
        }
    }
    return config;
}

/** Appends `added` to the list under `key`, creating the list the first time. */
inline void append_to(boost::json::object& lists, std::string_view key, boost::json::value added) {
    if (!lists.contains(key)) {
        lists[key] = boost::json::array{};
    }
    lists[key].as_array().push_back(std::move(added));
}

/**
 The `on` and `after` of a node's config from its written transitions: a
 delayed one goes back under its delay, every other under its event.
*/
inline result<void> transitions_config(const boost::json::object& written,
                                       boost::json::object& config) {
    const boost::json::value* on = written.if_contains("on");
    if (on == nullptr) {
        return {};
    }
    if (!on->is_object()) {
        return failure<void>(errc::invalid_config);
    }
    boost::json::object events;
    boost::json::object after;
    for (const auto& [descriptor, listed] : on->get_object()) {
        if (!listed.is_array()) {
            return failure<void>(errc::invalid_config);
        }
        for (const boost::json::value& one : listed.get_array()) {
            if (!one.is_object()) {
                return failure<void>(errc::invalid_config);
            }
            const boost::json::object& transition = one.get_object();
            if (const boost::json::value* delay = transition.if_contains("delay")) {
                const std::string key = delay->is_string() ? std::string(delay->get_string())
                                                           : boost::json::serialize(*delay);
                append_to(after, key, transition_config(transition));
                continue;
            }
            append_to(events, descriptor, transition_config(transition));
        }
    }
    if (!events.empty()) {
        config["on"] = std::move(events);
    }
    if (!after.empty()) {
        config["after"] = std::move(after);
    }
    return {};
}

/**
 A written invoke list as the config that writes it, without the type
 XState adds; the transitions of its `onDone` and `onError` come back
 through the node's `on`.
*/
inline result<boost::json::array> invoke_config(const boost::json::value* written) {
    boost::json::array config;
    if (written == nullptr) {
        return config;
    }
    if (!written->is_array()) {
        return failure<boost::json::array>(errc::invalid_config);
    }
    for (const boost::json::value& invoked : written->get_array()) {
        if (!invoked.is_object()) {
            return failure<boost::json::array>(errc::invalid_config);
        }
        boost::json::object one = invoked.get_object();
        one.erase("type");
        config.emplace_back(std::move(one));
    }
    return config;
}

/**
 A node's written initial transition as the config that writes it: the key
 of the child whose id its target names, with its actions, meta and
 description; none when it names no child.
*/
inline result<std::optional<boost::json::object>> initial_config(
    const boost::json::object& written) {
    const boost::json::value* initial = written.if_contains("initial");
    const boost::json::value* states = written.if_contains("states");
    if (initial == nullptr || !initial->is_object() || states == nullptr || !states->is_object()) {
        return std::optional<boost::json::object>();
    }
    const boost::json::value* target = initial->get_object().if_contains("target");
    if (target == nullptr || !target->is_array() || target->get_array().empty()) {
        return std::optional<boost::json::object>();
    }
    const boost::json::value& first = target->get_array().front();
    if (!first.is_string()) {
        return failure<std::optional<boost::json::object>>(errc::invalid_config);
    }
    const std::string target_id(first.get_string());
    std::optional<boost::json::object> config;
    for (const auto& [key, child] : states->get_object()) {
        const boost::json::value* id =
            child.is_object() ? child.get_object().if_contains("id") : nullptr;
        if (id == nullptr || !id->is_string() || "#" + std::string(id->get_string()) != target_id) {
            continue;
        }
        boost::json::object found{{"target", key}};
        const boost::json::value* actions = initial->get_object().if_contains("actions");
        if (actions != nullptr && actions->is_array() && !actions->get_array().empty()) {
            found["actions"] = *actions;
        }
        for (const char* carried : {"meta", "description"}) {
            if (const boost::json::value* value = initial->get_object().if_contains(carried)) {
                found[carried] = *value;
            }
        }
        config = std::move(found);
    }
    return config;
}

/** A node's own config from its written definition, its children left to the caller. */
inline result<boost::json::object> node_config(const boost::json::object& written) {
    boost::json::object config;
    for (const char* key : {"id", "type", "history", "meta", "description", "output", "tags"}) {
        if (const boost::json::value* value = written.if_contains(key)) {
            config[key] = *value;
        }
    }
    if (config.contains("history") && config.at("history").is_bool() &&
        !config.at("history").get_bool()) {
        config.erase("history");
    }
    for (const char* key : {"entry", "exit"}) {
        result<boost::json::array> actions = actions_config(written.if_contains(key));
        if (!actions.has_value()) {
            return actions.error();
        }
        config[key] = std::move(*actions);
    }
    if (const result<void> transitions = transitions_config(written, config);
        !transitions.has_value()) {
        return transitions.error();
    }
    result<boost::json::array> invoke = invoke_config(written.if_contains("invoke"));
    if (!invoke.has_value()) {
        return invoke.error();
    }
    if (!invoke->empty()) {
        config["invoke"] = std::move(*invoke);
    }
    result<std::optional<boost::json::object>> initial = initial_config(written);
    if (!initial.has_value()) {
        return initial.error();
    }
    if (std::optional<boost::json::object>& found = *initial; found.has_value()) {
        config["initial"] = std::move(*found);
    }
    return config;
}

}  // namespace detail

/**
 A machine loaded from a definition XState's machine.toJSON() wrote, with
 the context the definition does not hold.
*/
inline result<machine> create_machine_from_definition(const boost::json::value& definition,
                                                      implementations registry,
                                                      boost::json::value context) {
    if (!definition.is_object()) {
        return failure<machine>(errc::invalid_config);
    }
    result<boost::json::object> read = detail::node_config(definition.get_object());
    if (!read.has_value()) {
        return read.error();
    }
    boost::json::object root = std::move(*read);
    root["context"] = std::move(context);
    if (const boost::json::value* version = definition.get_object().if_contains("version")) {
        root["version"] = *version;
    }
    std::vector<std::pair<const boost::json::object*, boost::json::object*>> pending{
        {&definition.get_object(), &root},
    };
    while (!pending.empty()) {
        const auto [written, config] = pending.back();
        pending.pop_back();
        const boost::json::value* states = written->if_contains("states");
        if (states != nullptr && !states->is_object()) {
            return failure<machine>(errc::invalid_config);
        }
        if (states == nullptr || states->get_object().empty()) {
            continue;
        }
        boost::json::object& children = (*config)["states"].emplace_object();
        for (const auto& [key, child] : states->get_object()) {
            if (!child.is_object()) {
                return failure<machine>(errc::invalid_config);
            }
            result<boost::json::object> child_config = detail::node_config(child.get_object());
            if (!child_config.has_value()) {
                return child_config.error();
            }
            children[key] = std::move(*child_config);
        }
        for (const auto& [key, child] : states->get_object()) {
            pending.emplace_back(&child.get_object(), &children[key].as_object());
        }
    }
    return create_machine(root, std::move(registry));
}

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_DEFINITION_HPP
