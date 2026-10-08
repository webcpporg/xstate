// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 State values, XState's description of where a machine is, and the dotted
 paths that name them (utils.ts).

 A state value is JSON: the key of a compound state's active child, as
 `"green"`, or, when that child has children of its own or the state is
 parallel, an object from each active child's key to its own state value,
 as `{"walk": "slow"}` or `{"bold": "on", "italic": "off"}`.

 @see "The state value", in the guide.
 @see "Matching a state value", in the guide.
*/
#ifndef WEBCPP_XSTATE_STATE_VALUE_HPP
#define WEBCPP_XSTATE_STATE_VALUE_HPP

#include <boost/json.hpp>

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace webcpp::xstate {

/**
 Splits a state id into the keys of its path.

 It ports XState's `toStatePath`. The id is split at each dot, a backslash
 making the character after it part of the segment, so a key may hold a
 dot; an empty id is the path of one empty key:

 @code
 to_state_path("a\\.b.c");  // {"a.b", "c"}
 to_state_path("");         // {""}
 @endcode

 @param id The state id, or a dotted path.
 @return The segments, at least one.
*/
inline std::vector<std::string> to_state_path(std::string_view id) {
    std::vector<std::string> path;
    std::string segment;
    for (std::size_t index = 0; index < id.size(); ++index) {
        const char current = id[index];
        if (current == '\\' && index + 1 < id.size()) {
            segment += id[index + 1];
            ++index;
            continue;
        }
        if (current == '.') {
            path.push_back(segment);
            segment.clear();
            continue;
        }
        segment += current;
    }
    path.push_back(segment);
    return path;
}

/**
 Builds the state value a path describes.

 It ports XState's `pathToStateValue`: `["a", "b", "c"]` is
 `{"a": {"b": "c"}}`, `["a"]` is `"a"`, and an empty path is `{}`.

 @param path The keys from the root down.
 @return The state value.
*/
inline boost::json::value path_to_state_value(std::span<const std::string> path) {
    if (path.empty()) {
        return boost::json::object{};
    }
    boost::json::value value(path.back());
    for (std::size_t index = path.size() - 1; index > 0; --index) {
        boost::json::object wrapped;
        wrapped.emplace(path[index - 1], std::move(value));
        value = std::move(wrapped);
    }
    return value;
}

/**
 Converts a state value given as a dotted string to its object form.

 It ports XState's `toStateValue`: `"a.b.c"` is `{"a": {"b": "c"}}`, and
 any other value is returned as it is.

 @param given A state value, or a dotted string.
 @return The state value.
*/
inline boost::json::value to_state_value(const boost::json::value& given) {
    if (!given.is_string()) {
        return given;
    }
    const std::vector<std::string> path = to_state_path(given.get_string());
    return path_to_state_value(path);
}

/**
 Whether the state value `child` is `parent` or a state within it.

 It ports XState's `matchesState`, its recursion a list of the pairs still
 to compare. Either value may be a state value or a dotted string. The
 parent `"b"` matches the child `{"b": "b1", "c": "c1"}`, and the parent
 `"a.b"` matches the child `{"a": {"b": "c"}}`; the parent
 `{"a": {"b": "c"}}` does not match the child `"a.b"`.
 @ref snapshot::matches is this function with the snapshot's value as
 `child`.

 @param parent The state value that may hold `child`.
 @param child The state value that may lie within `parent`.
 @return `true` when `child` is `parent` or a state within it.
*/
inline bool matches_state(const boost::json::value& parent, const boost::json::value& child) {
    std::vector<std::pair<boost::json::value, boost::json::value>> pending;
    pending.emplace_back(to_state_value(parent), to_state_value(child));
    while (!pending.empty()) {
        const auto [parent_value, child_value] = std::move(pending.back());
        pending.pop_back();
        if (child_value.is_string()) {
            if (!parent_value.is_string() ||
                parent_value.get_string() != child_value.get_string()) {
                return false;
            }
            continue;
        }
        if (!child_value.is_object()) {
            return false;
        }
        const boost::json::object& child_object = child_value.get_object();
        if (parent_value.is_string()) {
            if (!child_object.contains(parent_value.get_string())) {
                return false;
            }
            continue;
        }
        if (!parent_value.is_object()) {
            return false;
        }
        for (const auto& member : parent_value.get_object()) {
            const boost::json::value* nested = child_object.if_contains(member.key());
            if (nested == nullptr) {
                return false;
            }
            pending.emplace_back(to_state_value(member.value()), to_state_value(*nested));
        }
    }
    return true;
}

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_STATE_VALUE_HPP
