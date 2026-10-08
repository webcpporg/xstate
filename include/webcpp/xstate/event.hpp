// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 An event, and XState's flat JSON form of it.

 @see "Events", in the guide.
*/
#ifndef WEBCPP_XSTATE_EVENT_HPP
#define WEBCPP_XSTATE_EVENT_HPP

#include <webcpp/xstate/errors.hpp>

#include <boost/json.hpp>

#include <string>
#include <string_view>

namespace webcpp::xstate {

/**
 The type of the event every initial macrostep runs with, "xstate.init".

 It ports XState's `XSTATE_INIT`. The event carries the machine's input as
 its member `input` when the input is not null.

 @note For an event of this type that a caller passes to @ref begin, the two
 libraries differ: XState's macrostep takes no transition for it, only the
 eventless transitions it then finds enabled, while xstate's @ref begin takes
 its transitions as any event's.

 @see "The `xstate.init` event", in the guide.
*/
inline constexpr std::string_view init_event_type = "xstate.init";

/**
 The descriptor that matches every event, "*".

 It ports XState's `WILDCARD`. A state's transitions under the `on` key
 `"*"` are candidates for any event, after those under the event's own type
 and those under a longer partial descriptor such as `"mouse.*"`. An event
 may have the wildcard as its type: a macrostep refuses one it takes from
 outside with @ref errc::invalid_event, as XState's development build does,
 while one the machine raises takes the wildcard transitions.

 @see "Event descriptors", in the guide.
*/
inline constexpr std::string_view wildcard = "*";

/**
 An event: its type, and the members that travel with it.

 It ports XState's `EventObject`. XState writes an event flat,
 `{"type": "SUBMIT", "user": "ana"}`; xstate keeps the type apart, and the
 rest is the payload, `{"user": "ana"}`, which every implementation reads as
 `args.event.payload`. A caller builds one as
 `xstate::event{.type = "SUBMIT", .payload = {{"user", "ana"}}}`.

 xstate makes some events itself: `xstate.init`, which runs the initial
 macrostep; `xstate.done.state.<id>`, raised when the state `<id>` reaches a
 final state, with the final state's `output` when it has one;
 `xstate.after.<delay>.<id>`, the delayed event of a state's `after`; and, in
 the actor layer, `xstate.done.actor.<id>` and `xstate.error.actor.<id>`,
 which a child sends its parent when it ends.

 @see "Events", in the guide.
*/
struct event {
    /** The type, which the transitions' event descriptors match. */
    std::string type{};

    /** Every other member of the event, in the order it was written. */
    boost::json::object payload{};
};

/**
 Reads an event from XState's flat JSON form.

 `json` is an object whose member `type` is a string, and every other member
 goes to the payload, in order.

 @note An event of the wildcard type is read like any other; only a
 macrostep that takes one from outside refuses it, as XState's does.

 @param json The event as XState writes it.
 @return The event; @ref errc::invalid_event when `json` is not an object,
 or has no `type`, or its `type` is not a string.
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

/**
 Writes an event in XState's flat JSON form.

 `type` comes first, then the payload's members in order. A payload member
 named `type` is not written, since the event's type holds that key.

 @param written The event.
 @return The event as XState writes it.
*/
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
