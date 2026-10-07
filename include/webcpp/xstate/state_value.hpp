// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 State values, XState's description of where a machine is: a key for a
 compound state's active child, an object for a parallel state's regions,
 nested; and the dotted paths that name them (utils.ts).
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
 Splits a state id at each dot, a backslash escaping the character after
 it; XState's toStatePath.

 Tip: "a\.b.c" is the path ["a.b", "c"], so a key may hold a dot.
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
 The state value a path describes, ["a", "b", "c"] being {"a": {"b": "c"}};
 XState's pathToStateValue.
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
 A state value in its object form when it was given as a dotted string;
 XState's toStateValue.
*/
inline boost::json::value to_state_value(const boost::json::value& given) {
    if (!given.is_string()) {
        return given;
    }
    const std::vector<std::string> path = to_state_path(given.get_string());
    return path_to_state_value(path);
}

/**
 Whether `child` is `parent` or a state within it; XState's matchesState,
 with its recursion as a list of the pairs still to compare.

 Tip: a string parent matches a child object that holds it as a key, so
 "b" matches {"b": "b1", "c": "c1"}.
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
