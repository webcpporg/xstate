// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 An event: its type and the payload that travels with it, read from and
 written to XState's flat JSON form, {"type": ..., ...payload}.
*/
#ifndef WEBCPP_XSTATE_EVENT_HPP
#define WEBCPP_XSTATE_EVENT_HPP

#include <webcpp/xstate/errors.hpp>

#include <boost/json.hpp>

#include <string>
#include <string_view>

namespace webcpp::xstate {

/** The type of the event every initial macrostep runs with (constants.ts). */
inline constexpr std::string_view init_event_type = "xstate.init";

/**
 The descriptor that matches every event. An event may have it as its type,
 which only a macrostep that takes one from outside refuses.
*/
inline constexpr std::string_view wildcard = "*";

struct event {
    std::string type{};
    boost::json::object payload{};
};

/**
 Reads an event from its flat JSON form: an object whose `type` is a
 string, the rest of its members being the payload.

 Tip: an event of the wildcard type is read like any other; only a
 macrostep that takes one from outside refuses it, as XState's does.
*/
inline result<event> event_from_json(const boost::json::value& json) {
    if (!json.is_object()) {
        return failure<event>(errc::invalid_event);
    }
    const boost::json::object& object = json.get_object();
    const boost::json::value* type = object.if_contains("type");
    if (type == nullptr || !type->is_string()) {
        return failure<event>(errc::invalid_event);
    }
    event read{.type = std::string(type->get_string()), .payload = {}};
    for (const auto& member : object) {
        if (member.key() != "type") {
            read.payload.emplace(member.key(), member.value());
        }
    }
    return read;
}

/** Writes an event in XState's flat form, its type first. */
inline boost::json::object to_json(const event& written) {
    boost::json::object json;
    json.emplace("type", written.type);
    for (const auto& member : written.payload) {
        json.emplace(member.key(), member.value());
    }
    return json;
}

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_EVENT_HPP
