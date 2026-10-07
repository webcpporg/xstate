// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The messages xstate's actors exchange on xactor: the start, an event, the
 resume that continues a parked actor, a delayed sendTo its sender forwards
 when its timer fires, the host's answer to a host actor, the stop, and a
 child's construction with its reply (doc: #reference-message-hpp).

 Tip: xactor names a message's alternative in its log by its index, so the
 order of the alternatives is part of a recorded run.
*/
#ifndef WEBCPP_XSTATE_ACTORS_MESSAGE_HPP
#define WEBCPP_XSTATE_ACTORS_MESSAGE_HPP

#include <webcpp/xactor/ids.hpp>
#include <webcpp/xstate/event.hpp>

#include <boost/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace webcpp::xstate {

using xactor::actor_ref;

/**
 Starts the actor: hands over what its initial macrostep returned and runs
 what it deferred, constructing it first if no parent did; XState's
 actor.start().
*/
struct start_actor {};

/** Continues an actor the fuel of an earlier execution could not finish. */
struct resume {};

/**
 A delayed sendTo whose timer fired: its sender forwards `event` to `to`.

 Tip: xactor releases a timer to the actor that armed it, as XState's
 scheduler keys a delayed event by its source; the target was resolved when
 the sendTo was.
*/
struct relay {
    actor_ref to{};
    xstate::event event{};
    // The id it was armed under, which its firing unregisters.
    std::optional<std::string> send_id{};
};

/**
 A delayed event to the actor itself, armed under an id: its firing
 unregisters the id, as XState's scheduler deletes its entry of timerMap,
 then the actor takes the event.
*/
struct delayed {
    std::string send_id{};
    xstate::event event{};
};

/**
 The host's answer to a host actor: its output, or its error; none, as a
 promise settled with undefined.
*/
struct host_answer {
    bool resolved = false;
    std::optional<boost::json::value> value{};
};

/**
 Asks an actor to stop with its family once it has handled what its parent
 sent it before; XState's deferred stopChild, which runs after the sends of
 the same macrostep.
*/
struct stop_actor {};

/**
 Constructs a machine child: runs its initial macrostep and resolves its
 actions, then answers `constructed`; XState's createActor, which its
 parent's resolveSpawn calls.
*/
struct construct_actor {};

/** A child's answer to `construct_actor`, once its own children are constructed too. */
struct constructed {};

using actor_message = std::variant<start_actor, event, resume, relay, host_answer, stop_actor,
                                   construct_actor, constructed, delayed>;

/**
 The event an actor that ends sends its parent: `kind` "done" with its
 output, or "error" with its error, and its id; XState's
 createDoneActorEvent and createErrorActorEvent.
*/
inline event ending_event(std::string_view kind, std::string_view id, std::string_view member,
                          const std::optional<boost::json::value>& value) {
    event ending{
        .type = "xstate." + std::string(kind) + ".actor." + std::string(id),
        .payload = {},
    };
    if (value.has_value()) {
        ending.payload.emplace(member, *value);
    }
    ending.payload.emplace("actorId", id);
    return ending;
}

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_ACTORS_MESSAGE_HPP
