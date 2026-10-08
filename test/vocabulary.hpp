// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The implementations xstate's test cases may name, in C++: the same table as
 test/oracle/vocabulary.mjs, entry for entry, which XState runs.

 Tip: a case declares its raise, log, sendParent, cancel, spawnChild,
 stopChild, sendTo, forwardTo and emit actions with their options under
 "actions", because XState takes an id, a delay and a target as fixed
 values.
*/
#ifndef WEBCPP_TEST_XSTATE_VOCABULARY_HPP
#define WEBCPP_TEST_XSTATE_VOCABULARY_HPP

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace webcpp::test::xstate_vocabulary {

namespace xstate = webcpp::xstate;

/** A JSON number as a double, as JavaScript holds every number. */
inline double number_of(const boost::json::value& value) {
    if (value.is_int64()) {
        return static_cast<double>(value.get_int64());
    }
    if (value.is_uint64()) {
        return static_cast<double>(value.get_uint64());
    }
    return value.is_double() ? value.get_double() : 0.0;
}

/** A sum held as an integer when it is one, as JSON.stringify writes it. */
inline boost::json::value sum_of(const boost::json::value& left, const boost::json::value& right) {
    if (left.is_int64() && right.is_int64()) {
        return left.get_int64() + right.get_int64();
    }
    return number_of(left) + number_of(right);
}

/** A member of an object, or null. */
inline boost::json::value member_of(const boost::json::value& object, std::string_view key) {
    if (!object.is_object()) {
        return nullptr;
    }
    const boost::json::value* found = object.get_object().if_contains(key);
    return found == nullptr ? boost::json::value() : *found;
}

/** The string a member names, or the empty string. */
inline std::string key_of(const boost::json::value& params) {
    const boost::json::value key = member_of(params, "key");
    return key.is_string() ? std::string(key.get_string()) : std::string();
}

/** A member of an event, its type included, as the flat object XState sees. */
inline boost::json::value event_member(const xstate::event& happened, std::string_view key) {
    if (key == "type") {
        return boost::json::string(happened.type);
    }
    const boost::json::value* found = happened.payload.if_contains(key);
    return found == nullptr ? boost::json::value() : *found;
}

/** The code points of UTF-8 text, as UTF-16 code units count them: one, or two past 0xFFFF. */
inline std::vector<std::pair<std::string_view, int>> utf16_characters(std::string_view text) {
    std::vector<std::pair<std::string_view, int>> characters;
    std::size_t at = 0;
    while (at < text.size()) {
        const auto lead = static_cast<unsigned char>(text[at]);
        std::size_t length = 1;
        if (lead >= 0xF0U) {
            length = 4;
        } else if (lead >= 0xE0U) {
            length = 3;
        } else if (lead >= 0xC0U) {
            length = 2;
        }
        length = std::min(length, text.size() - at);
        characters.emplace_back(text.substr(at, length), length == 4 ? 2 : 1);
        at += length;
    }
    return characters;
}

/** A canonical array index, as JavaScript reads "0", "12", but not "01" or "-1". */
inline std::optional<std::size_t> index_of(std::string_view from) {
    std::size_t index = 0;
    const auto [end, failed] = std::from_chars(from.data(), from.data() + from.size(), index);
    if (failed != std::errc() || end != from.data() + from.size() || from.empty() ||
        (from.size() > 1 && from.front() == '0')) {
        return std::nullopt;
    }
    return index;
}

/** What JavaScript's text[from] reads of a string: its UTF-16 length, or one code unit as text. */
inline xstate::result<std::optional<boost::json::value>> text_member(std::string_view text,
                                                                     std::string_view from) {
    const std::vector<std::pair<std::string_view, int>> characters = utf16_characters(text);
    if (from == "length") {
        std::int64_t units = 0;
        for (const auto& character : characters) {
            units += character.second;
        }
        return boost::json::value(units);
    }
    const std::optional<std::size_t> index = index_of(from);
    if (!index.has_value()) {
        return std::nullopt;
    }
    std::size_t unit = 0;
    for (const auto& [character, units] : characters) {
        if (*index < unit + static_cast<std::size_t>(units)) {
            // Half of a surrogate pair is no text JSON can carry.
            return units == 1 ? xstate::result<std::optional<boost::json::value>>(
                                    boost::json::value(boost::json::string(character)))
                              : xstate::failure<std::optional<boost::json::value>>(
                                    xstate::errc::implementation_failed);
        }
        unit += static_cast<std::size_t>(units);
    }
    return std::nullopt;
}

/**
 What JavaScript's value.__proto__ reads of a value that has no own member
 of that name: its prototype, as JSON.stringify writes it.
*/
inline std::optional<boost::json::value> prototype_of(const boost::json::value& value) {
    if (value.is_object()) {
        return boost::json::value(boost::json::object());
    }
    if (value.is_array()) {
        return boost::json::value(boost::json::array());
    }
    if (value.is_string()) {
        return boost::json::value(boost::json::string());
    }
    if (value.is_number()) {
        return boost::json::value(0);
    }
    if (value.is_bool()) {
        return boost::json::value(false);
    }
    return std::nullopt;
}

/**
 The member `from` names of a value, as JavaScript's value[from] reads it:
 an object's own member; an array's element by index or its length; a
 string's character by UTF-16 index or its UTF-16 length; for "__proto__",
 the prototype; none for anything else. Half of a surrogate pair fails, as
 the vocabulary's functions do.
*/
inline xstate::result<std::optional<boost::json::value>> js_member(const boost::json::value& value,
                                                                   std::string_view from) {
    if (value.is_object()) {
        if (const boost::json::value* found = value.get_object().if_contains(from)) {
            return std::optional(*found);
        }
    }
    if (from == "__proto__") {
        return prototype_of(value);
    }
    if (value.is_array()) {
        const boost::json::array& items = value.get_array();
        if (from == "length") {
            return boost::json::value(static_cast<std::int64_t>(items.size()));
        }
        const std::optional<std::size_t> index = index_of(from);
        return index.has_value() && *index < items.size() ? std::optional(items[*index])
                                                          : std::nullopt;
    }
    if (value.is_string()) {
        return text_member(value.get_string(), from);
    }
    return std::nullopt;
}

/**
 The member a dotted path names inside a value, the whole value for "";
 none when one is missing.
*/
inline xstate::result<std::optional<boost::json::value>> at_path(boost::json::value value,
                                                                 std::string_view path) {
    while (!path.empty()) {
        const std::size_t dot = path.find('.');
        xstate::result<std::optional<boost::json::value>> inside =
            js_member(value, path.substr(0, dot));
        if (!inside.has_value() || !inside->has_value()) {
            return inside;
        }
        value = std::move(**inside);
        path = dot == std::string_view::npos ? std::string_view() : path.substr(dot + 1);
    }
    return std::optional(std::move(value));
}

/** JavaScript's property key of a JSON value: a string, or what String() writes of the rest. */
inline std::string property_key(const boost::json::value& from) {
    if (from.is_string()) {
        return std::string(from.get_string());
    }
    if (from.is_bool()) {
        return from.get_bool() ? "true" : "false";
    }
    if (from.is_null()) {
        return "null";
    }
    return boost::json::serialize(from);
}

/** What JSON.stringify writes of a scalar, every number as its double, -0 as 0. */
inline std::string js_scalar(const boost::json::value& value) {
    if (!value.is_number()) {
        return boost::json::serialize(value);
    }
    double number = number_of(value);
    if (number == 0.0) {
        number = 0.0;
    }
    std::array<char, 32> buffer{};
    const auto [end, failed] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
    return {buffer.data(), failed == std::errc() ? end : buffer.data()};
}

/** Each value to write after its prefix, and, as a null value, a closing mark. */
using json_work = std::vector<std::pair<const boost::json::value*, std::string>>;

/** Queues an object's or an array's members, opening and closing it; false for a scalar. */
inline bool queue_members(const boost::json::value& value, std::string& written, json_work& work) {
    if (value.is_object()) {
        written += '{';
        work.emplace_back(nullptr, "}");
        json_work members;
        for (const auto& member : value.get_object()) {
            std::string prefix = members.empty() ? "" : ",";
            prefix += boost::json::serialize(boost::json::value(boost::json::string(member.key())));
            prefix += ':';
            members.emplace_back(&member.value(), std::move(prefix));
        }
        work.insert(work.end(), std::make_move_iterator(members.rbegin()),
                    std::make_move_iterator(members.rend()));
        return true;
    }
    if (value.is_array()) {
        written += '[';
        work.emplace_back(nullptr, "]");
        const boost::json::array& items = value.get_array();
        for (std::size_t index = items.size(); index > 0; --index) {
            work.emplace_back(&items[index - 1], index == 1 ? "" : ",");
        }
        return true;
    }
    return false;
}

/**
 What JSON.stringify writes of a value, none for undefined, for equality
 only: objects in their order, every number as its double, -0 as 0.

 Tip: an explicit stack in place of recursion.
*/
inline std::optional<std::string> js_json(const std::optional<boost::json::value>& value) {
    if (!value.has_value()) {
        return std::nullopt;
    }
    std::string written;
    json_work work{{&*value, ""}};
    while (!work.empty()) {
        auto [next, prefix] = std::move(work.back());
        work.pop_back();
        written += prefix;
        if (next != nullptr && !queue_members(*next, written, work)) {
            written += js_scalar(*next);
        }
    }
    return written;
}

/** Whether two values are equal as JSON.stringify writes them. */
inline bool same_json(const std::optional<boost::json::value>& left,
                      const std::optional<boost::json::value>& right) {
    return js_json(left) == js_json(right);
}

/** Whether an object is one of the vocabulary's operators: one "$" key. */
inline bool is_operator(const boost::json::value& expression) {
    return expression.is_object() && expression.get_object().size() == 1 &&
           expression.get_object().begin()->key().starts_with('$');
}

/**
 The values an expression is built from: an array's items, an object's
 members, and the operands of "$sum", "$product" and "$concat".
*/
inline std::vector<const boost::json::value*> parts_of(const boost::json::value& expression) {
    std::vector<const boost::json::value*> parts;
    if (expression.is_array()) {
        for (const boost::json::value& item : expression.get_array()) {
            parts.push_back(&item);
        }
    } else if (is_operator(expression)) {
        const boost::json::value& operand = expression.get_object().begin()->value();
        if (operand.is_array()) {
            for (const boost::json::value& item : operand.get_array()) {
                parts.push_back(&item);
            }
        }
    } else if (expression.is_object()) {
        for (const auto& member : expression.get_object()) {
            parts.push_back(&member.value());
        }
    }
    return parts;
}

/** Whether a value written in a case computes something, as vocabulary.mjs's isComputed. */
inline bool is_computed(const boost::json::value& expression) {
    std::vector<const boost::json::value*> pending{&expression};
    while (!pending.empty()) {
        const boost::json::value* next = pending.back();
        pending.pop_back();
        if (is_operator(*next)) {
            return true;
        }
        for (const boost::json::value* part : parts_of(*next)) {
            pending.push_back(part);
        }
    }
    return false;
}

/** JavaScript's String(value), for the values a case computes. */
inline std::string text_of(const boost::json::value& value) {
    if (value.is_string()) {
        return std::string(value.get_string());
    }
    if (value.is_bool()) {
        return value.get_bool() ? "true" : "false";
    }
    if (value.is_null()) {
        return "null";
    }
    return boost::json::serialize(value);
}

/** What an operator gives from the values its operands computed, as vocabulary.mjs's computed. */
inline xstate::result<std::optional<boost::json::value>> operator_value(
    const boost::json::value& expression,
    std::span<const std::optional<boost::json::value>> operands, const xstate::action_args& args) {
    const std::string_view name = expression.get_object().begin()->key();
    const boost::json::value& operand = expression.get_object().begin()->value();
    if (name == "$context" && operand.is_string()) {
        return at_path(args.context, operand.get_string());
    }
    if (name == "$event" && operand.is_string()) {
        return at_path(xstate::to_json(args.event), operand.get_string());
    }
    if (name == "$concat") {
        std::string joined;
        for (const std::optional<boost::json::value>& one : operands) {
            joined += text_of(one.value_or(boost::json::value()));
        }
        return boost::json::value(boost::json::string(joined));
    }
    if (name == "$sum" || name == "$product") {
        const bool sum = name == "$sum";
        boost::json::value total = sum ? boost::json::value(0) : boost::json::value(1);
        for (const std::optional<boost::json::value>& one : operands) {
            const boost::json::value value = one.value_or(boost::json::value());
            if (sum) {
                total = sum_of(total, value);
            } else {
                total = total.is_int64() && value.is_int64()
                            ? boost::json::value(total.get_int64() * value.get_int64())
                            : boost::json::value(number_of(total) * number_of(value));
            }
        }
        return total;
    }
    return xstate::failure<std::optional<boost::json::value>>(xstate::errc::invalid_config);
}

/** What an expression gives from the values its parts computed. */
inline xstate::result<std::optional<boost::json::value>> built_value(
    const boost::json::value& expression, std::span<const std::optional<boost::json::value>> parts,
    const xstate::action_args& args) {
    if (is_operator(expression)) {
        return operator_value(expression, parts, args);
    }
    if (expression.is_array()) {
        boost::json::array items;
        for (const std::optional<boost::json::value>& part : parts) {
            items.push_back(part.value_or(boost::json::value()));
        }
        return boost::json::value(std::move(items));
    }
    if (!expression.is_object()) {
        return expression;
    }
    boost::json::object built;
    std::size_t index = 0;
    for (const auto& member : expression.get_object()) {
        if (parts[index].has_value()) {
            built.emplace(member.key(), *parts[index]);
        }
        ++index;
    }
    return boost::json::value(std::move(built));
}

/**
 A value of the case vocabulary computed from the context and the event, as
 vocabulary.mjs's computed: a literal is itself, "$context" and "$event"
 read a member by a dotted path, "$sum", "$product" and "$concat" combine
 what they hold, and an array or object computes each member. None stands
 for JavaScript's undefined, which an object drops and an array writes as
 null.

 Tip: the expression is computed parts first, an explicit stack in place of
 recursion.
*/
inline xstate::result<std::optional<boost::json::value>> computed_value(
    const boost::json::value& expression, const xstate::action_args& args) {
    std::vector<const boost::json::value*> order;
    std::vector<std::pair<const boost::json::value*, bool>> pending{{&expression, false}};
    while (!pending.empty()) {
        const auto [next, expanded] = pending.back();
        pending.pop_back();
        if (expanded) {
            order.push_back(next);
            continue;
        }
        pending.emplace_back(next, true);
        const std::vector<const boost::json::value*> parts = parts_of(*next);
        for (const boost::json::value* part : std::ranges::reverse_view(parts)) {
            pending.emplace_back(part, false);
        }
    }
    std::vector<std::optional<boost::json::value>> values;
    for (const boost::json::value* next : order) {
        const std::size_t count = parts_of(*next).size();
        const auto first = values.end() - static_cast<std::ptrdiff_t>(count);
        xstate::result<std::optional<boost::json::value>> made = built_value(
            *next, std::span<const std::optional<boost::json::value>>(first, values.end()), args);
        if (!made.has_value()) {
            return made;
        }
        values.erase(first, values.end());
        values.push_back(std::move(*made));
    }
    return std::move(values.back());
}

/** A computed value, JavaScript's undefined written as null. */
inline xstate::result<boost::json::value> computed(const boost::json::value& expression,
                                                   const xstate::action_args& args) {
    xstate::result<std::optional<boost::json::value>> made = computed_value(expression, args);
    if (!made.has_value()) {
        return made.error();
    }
    return made->value_or(boost::json::value());
}

/** A maker of a value a case computes, as vocabulary.mjs's computed; none for undefined. */
inline xstate::value_maker computing(boost::json::value expression) {
    return [expression = std::move(expression)](const xstate::action_args& args) {
        return computed_value(expression, args);
    };
}

/** A delay a declared action names: a number, or a delay's name. */
inline std::optional<xstate::delay_ref> declared_delay(const boost::json::object& spec) {
    const boost::json::value* delay = spec.if_contains("delay");
    if (delay == nullptr) {
        return std::nullopt;
    }
    if (delay->is_string()) {
        return xstate::delay_ref{std::string(delay->get_string())};
    }
    return xstate::delay_ref{static_cast<std::uint64_t>(number_of(*delay))};
}

/** An optional string member of a declared action. */
inline std::optional<std::string> declared_string(const boost::json::object& spec,
                                                  std::string_view key) {
    const boost::json::value* found = spec.if_contains(key);
    if (found == nullptr || !found->is_string()) {
        return std::nullopt;
    }
    return std::string(found->get_string());
}

/** An event a declared action names, fixed or computed, as a maker returning it. */
inline xstate::event_maker fixed_event(const boost::json::value& json) {
    return [json](const xstate::action_args& args) -> xstate::result<xstate::event> {
        xstate::result<boost::json::value> made = computed(json, args);
        if (!made.has_value()) {
            return made.error();
        }
        return xstate::event_from_json(*made);
    };
}

/**
 A declared spawnChild: its src, id and systemId, and its input, fixed or
 read from a member of the context.
*/
inline xstate::spawn_child_action declared_spawn(const boost::json::object& spec) {
    xstate::spawn_child_action spawned{
        .src = declared_string(spec, "src").value_or(""),
        .id = declared_string(spec, "id").value_or(""),
        .system_id = declared_string(spec, "systemId"),
        .input = std::nullopt,
    };
    if (const boost::json::value* id = spec.if_contains("id"); id != nullptr && is_computed(*id)) {
        spawned.id = [expression =
                          *id](const xstate::action_args& args) -> xstate::result<std::string> {
            xstate::result<boost::json::value> made = computed(expression, args);
            if (!made.has_value()) {
                return made.error();
            }
            return text_of(*made);
        };
    }
    if (const std::optional<std::string> key = declared_string(spec, "input_from_context")) {
        // context[key], undefined when the context has no such member.
        spawned.input = [key = *key](const xstate::action_args& args)
            -> xstate::result<std::optional<boost::json::value>> {
            const boost::json::value* found =
                args.context.is_object() ? args.context.get_object().if_contains(key) : nullptr;
            return found == nullptr ? std::nullopt : std::optional(*found);
        };
    } else if (const boost::json::value* input = spec.if_contains("input")) {
        spawned.input = computing(*input);
    }
    return spawned;
}

/** A declared action that names another actor, as the built-in it stands for. */
inline std::optional<xstate::action_implementation> declared_actor_action(
    const boost::json::object& spec) {
    const std::optional<std::string> kind = declared_string(spec, "kind");
    const boost::json::value event =
        spec.contains("event") ? spec.at("event") : boost::json::value();
    if (kind == "spawn_child") {
        return declared_spawn(spec);
    }
    if (kind == "stop_child") {
        return xstate::stop_child_action{.id = declared_string(spec, "id").value_or("")};
    }
    if (kind == "send_to") {
        return xstate::send_to_action{
            // No target is the actor itself, as XState's sendTo(undefined).
            .target = declared_string(spec, "target").value_or("#_internal"),
            .event = fixed_event(event),
            .id = declared_string(spec, "id"),
            .delay = declared_delay(spec),
        };
    }
    if (kind == "forward_to") {
        return xstate::forward_to_action{
            .target = declared_string(spec, "target").value_or(""),
            .id = declared_string(spec, "id"),
            .delay = declared_delay(spec),
        };
    }
    if (kind == "emit") {
        return xstate::emit_action{.event = fixed_event(event)};
    }
    return std::nullopt;
}

/** One declared action of a case, as the built-in it stands for. */
inline std::optional<xstate::action_implementation> declared(const boost::json::object& spec) {
    const std::optional<std::string> kind = declared_string(spec, "kind");
    const boost::json::value event =
        spec.contains("event") ? spec.at("event") : boost::json::value();
    if (kind == "raise") {
        return xstate::raise_action{
            .event = fixed_event(event),
            .id = declared_string(spec, "id"),
            .delay = declared_delay(spec),
        };
    }
    if (kind == "send_parent") {
        return xstate::send_parent_action{
            .event = fixed_event(event),
            .id = declared_string(spec, "id"),
            .delay = declared_delay(spec),
        };
    }
    if (kind == "cancel") {
        return xstate::cancel_action{.id = declared_string(spec, "id").value_or("")};
    }
    if (kind == "log") {
        xstate::log_action logged{.value = std::nullopt, .label = declared_string(spec, "label")};
        if (const boost::json::value* value = spec.if_contains("value")) {
            logged.value = computing(*value);
        }
        return logged;
    }
    return declared_actor_action(spec);
}

/** A failure where XState's implementation throws. */
template <class T>
xstate::result<T> fail() {
    return xstate::failure<T>(xstate::errc::implementation_failed);
}

/**
 The key an entry's params name, or none when they name none.

 Tip: the vocabulary is strict, as vocabulary.mjs is: an entry given what it
 is not for fails, where JavaScript would coerce or throw its own TypeError.
*/
inline std::optional<std::string> required_key(const boost::json::value& params) {
    const boost::json::value* key =
        params.is_object() ? params.get_object().if_contains("key") : nullptr;
    return key != nullptr && key->is_string() ? std::optional(std::string(key->get_string()))
                                              : std::nullopt;
}

/** A member of an object, none when it has none: JavaScript's undefined. */
inline std::optional<boost::json::value> present(const boost::json::value& object,
                                                 std::string_view key) {
    const boost::json::value* found =
        object.is_object() ? object.get_object().if_contains(key) : nullptr;
    return found == nullptr ? std::nullopt : std::optional(*found);
}

/** Whether a value is a finite number, as the vocabulary's numeric entries need. */
inline bool is_number(const std::optional<boost::json::value>& value) {
    return value.has_value() && value->is_number() && std::isfinite(number_of(*value));
}

/** A list in the context with one value appended, or a failure when it is not a list. */
inline xstate::result<boost::json::object> appended(const xstate::action_args& args,
                                                    boost::json::value added) {
    const std::optional<std::string> key = required_key(args.params);
    if (!key.has_value()) {
        return fail<boost::json::object>();
    }
    boost::json::value list = member_of(args.context, *key);
    if (!list.is_array()) {
        return fail<boost::json::object>();
    }
    list.get_array().push_back(std::move(added));
    boost::json::object update;
    update.emplace(*key, std::move(list));
    return update;
}

/** One update of a key, or the vocabulary's failure without a key. */
inline xstate::result<boost::json::object> updated(const xstate::action_args& args,
                                                   std::optional<boost::json::value> value) {
    const std::optional<std::string> key = required_key(args.params);
    if (!key.has_value() || !value.has_value()) {
        return fail<boost::json::object>();
    }
    boost::json::object update;
    update.emplace(*key, std::move(*value));
    return update;
}

/** A sum of two finite numbers, an integer while it fits one, as JavaScript's + gives. */
inline boost::json::value sum_checked(const boost::json::value& left,
                                      const boost::json::value& right) {
    if (left.is_int64() && right.is_int64()) {
        const std::int64_t l = left.get_int64();
        const std::int64_t r = right.get_int64();
        const bool overflows = (r > 0 && l > std::numeric_limits<std::int64_t>::max() - r) ||
                               (r < 0 && l < std::numeric_limits<std::int64_t>::min() - r);
        if (!overflows) {
            return l + r;
        }
    }
    return number_of(left) + number_of(right);
}

/** The assigns of the vocabulary. */
inline void add_assigns(xstate::implementations& registry) {
    const auto set = [](const xstate::action_args& args) {
        return updated(args, present(args.params, "value"));
    };
    const auto increment =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const std::optional<std::string> key = required_key(args.params);
        const std::optional<boost::json::value> held =
            key.has_value() ? present(args.context, *key) : std::nullopt;
        const std::optional<boost::json::value> by =
            present(args.params, "by").value_or(boost::json::value(1));
        if (!is_number(held) || !is_number(by)) {
            return fail<boost::json::object>();
        }
        return updated(args, sum_checked(*held, *by));
    };
    const auto push = [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        xstate::result<boost::json::value> value = computed(member_of(args.params, "value"), args);
        if (!value.has_value()) {
            return value.error();
        }
        return appended(args, std::move(*value));
    };
    const auto set_computed =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        xstate::result<std::optional<boost::json::value>> value =
            computed_value(member_of(args.params, "value"), args);
        if (!value.has_value()) {
            return value.error();
        }
        return updated(args, std::move(*value));
    };
    const auto push_event = [](const xstate::action_args& args) {
        return appended(args, boost::json::string(args.event.type));
    };
    // A member the event lacks changes nothing.
    const auto set_from_event =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::value from = member_of(args.params, "from");
        if (!required_key(args.params).has_value() || !from.is_string()) {
            return fail<boost::json::object>();
        }
        xstate::result<std::optional<boost::json::value>> copied =
            at_path(xstate::to_json(args.event), from.get_string());
        if (!copied.has_value()) {
            return copied.error();
        }
        return copied->has_value() ? updated(args, std::move(*copied))
                                   : xstate::result<boost::json::object>(boost::json::object());
    };
    const auto failing = [](const xstate::action_args&) { return fail<boost::json::object>(); };
    registry.actions.emplace("set", xstate::assign_action{.assignment = set});
    registry.actions.emplace("increment", xstate::assign_action{.assignment = increment});
    registry.actions.emplace("push", xstate::assign_action{.assignment = push});
    registry.actions.emplace("set_computed", xstate::assign_action{.assignment = set_computed});
    registry.actions.emplace("push_event", xstate::assign_action{.assignment = push_event});
    registry.actions.emplace("set_from_event", xstate::assign_action{.assignment = set_from_event});
    registry.actions.emplace("fail", xstate::assign_action{.assignment = failing});
}

/** Two numbers compared by `op`, the vocabulary's failure for anything else. */
inline xstate::result<bool> ordered(const std::optional<boost::json::value>& left,
                                    std::string_view op,
                                    const std::optional<boost::json::value>& right) {
    if (!is_number(left) || !is_number(right)) {
        return fail<bool>();
    }
    const double first = number_of(*left);
    const double second = number_of(*right);
    if (op == "<") {
        return first < second;
    }
    if (op == "<=") {
        return first <= second;
    }
    if (op == ">") {
        return first > second;
    }
    return first >= second;
}

/** The guards of the vocabulary. */
inline void add_guards(xstate::implementations& registry) {
    registry.guards.emplace(
        "context_equals", [](const xstate::action_args& args) -> xstate::result<bool> {
            const std::optional<std::string> key = required_key(args.params);
            if (!key.has_value()) {
                return fail<bool>();
            }
            return same_json(present(args.context, *key), present(args.params, "value"));
        });
    registry.guards.emplace(
        "context_at_least", [](const xstate::action_args& args) -> xstate::result<bool> {
            const std::optional<std::string> key = required_key(args.params);
            if (!key.has_value()) {
                return fail<bool>();
            }
            return ordered(present(args.context, *key), ">=", present(args.params, "value"));
        });
    registry.guards.emplace("event_equals",
                            [](const xstate::action_args& args) -> xstate::result<bool> {
                                const std::optional<std::string> key = required_key(args.params);
                                if (!key.has_value()) {
                                    return fail<bool>();
                                }
                                return same_json(present(xstate::to_json(args.event), *key),
                                                 present(args.params, "value"));
                            });
    registry.guards.emplace("compare", [](const xstate::action_args& args) -> xstate::result<bool> {
        if (!args.params.is_object()) {
            return fail<bool>();
        }
        xstate::result<std::optional<boost::json::value>> left =
            computed_value(member_of(args.params, "left"), args);
        xstate::result<std::optional<boost::json::value>> right =
            computed_value(member_of(args.params, "right"), args);
        if (!left.has_value() || !right.has_value()) {
            return fail<bool>();
        }
        const boost::json::value op = member_of(args.params, "op");
        const std::string_view written = op.is_string() ? op.get_string() : std::string_view();
        if (written == "==") {
            return same_json(*left, *right);
        }
        if (written != "<" && written != "<=" && written != ">" && written != ">=") {
            return fail<bool>();
        }
        return ordered(*left, written, *right);
    });
    registry.guards.emplace("fail", [](const xstate::action_args&) { return fail<bool>(); });
}

/**
 A case's context from the machine's input, as a map of each context key to
 the member of the input it takes, which a context lacks when the input does;
 a missing input fails, as reading a member of undefined throws in the
 function it stands for.
*/
inline xstate::context_maker context_from_input(boost::json::object mapping) {
    return [mapping = std::move(mapping)](
               const boost::json::value& input) -> xstate::result<boost::json::value> {
        if (input.is_null()) {
            return fail<boost::json::value>();
        }
        boost::json::object context;
        for (const auto& [key, from] : mapping) {
            xstate::result<std::optional<boost::json::value>> taken =
                js_member(input, property_key(from));
            if (!taken.has_value()) {
                return taken.error();
            }
            if (taken->has_value()) {
                context.emplace(key, std::move(**taken));
            }
        }
        return boost::json::value(std::move(context));
    };
}

/** A case's delays, each a whole number of milliseconds, fixed or computed. */
inline void add_delays(const boost::json::object& the_case, xstate::implementations& registry) {
    const boost::json::value* delays = the_case.if_contains("delays");
    if (delays == nullptr || !delays->is_object()) {
        return;
    }
    for (const auto& [name, delay] : delays->get_object()) {
        registry.delays.emplace(
            std::string(name),
            [expression = delay](const xstate::action_args& args) -> xstate::result<std::uint64_t> {
                xstate::result<boost::json::value> made = computed(expression, args);
                if (!made.has_value()) {
                    return made.error();
                }
                // A whole number of milliseconds, xstate's clock's unit.
                const double milliseconds = number_of(*made);
                if (!made->is_number() || milliseconds < 0 ||
                    milliseconds != std::floor(milliseconds)) {
                    return fail<std::uint64_t>();
                }
                return static_cast<std::uint64_t>(milliseconds);
            });
    }
}

/** A case's computed inputs, by invoke id, and outputs, by state id. */
inline void add_computed(const boost::json::object& the_case, xstate::implementations& registry) {
    for (const auto& [field, into] :
         {std::pair{"inputs", &registry.inputs}, std::pair{"outputs", &registry.outputs}}) {
        const boost::json::value* computed_ones = the_case.if_contains(field);
        if (computed_ones == nullptr || !computed_ones->is_object()) {
            continue;
        }
        for (const auto& [id, expression] : computed_ones->get_object()) {
            into->emplace(std::string(id), computing(expression));
        }
    }
}

/** The implementations one case may name: the vocabulary, its declared actions and its delays. */
inline xstate::implementations of_case(const boost::json::object& the_case) {
    xstate::implementations registry;
    add_assigns(registry);
    add_guards(registry);
    if (const boost::json::value* mapping = the_case.if_contains("context_from_input");
        mapping != nullptr && mapping->is_object()) {
        registry.context = context_from_input(mapping->get_object());
    }
    if (const boost::json::value* actions = the_case.if_contains("actions"); actions != nullptr) {
        for (const auto& [name, spec] : actions->get_object()) {
            if (std::optional<xstate::action_implementation> one = declared(spec.get_object())) {
                registry.actions.insert_or_assign(std::string(name), std::move(*one));
            }
        }
    }
    // Every actor a case declares is a host actor here: the pure functions run
    // no child.
    if (const boost::json::value* actors = the_case.if_contains("actors"); actors != nullptr) {
        for (const auto& [name, spec] : actors->get_object()) {
            registry.actors.emplace(std::string(name), xstate::host_actor{});
        }
    }
    add_delays(the_case, registry);
    add_computed(the_case, registry);
    return registry;
}

}  // namespace webcpp::test::xstate_vocabulary

#endif  // WEBCPP_TEST_XSTATE_VOCABULARY_HPP
