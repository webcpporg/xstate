// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The messages xstate's actors exchange on xactor: the start, an event, the
 resume that continues a parked actor, a delayed sendTo its sender forwards
 when its timer fires, the host's answer to a host actor, the stop, and a
 child's construction with its reply.

 @note xactor names a message's alternative in its log by its index, so the
 order of the alternatives is part of a recorded run.

 @see "How the layer runs", in the guide.
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

/**
 An actor's address in its system: `webcpp::xactor::actor_ref`, brought
 into this namespace.

 Its one member, `value`, is a number, not a handle: it keeps nothing alive,
 and @ref actor_system::status_of tells whether its actor still runs.
 Addresses start at 1, and 0 names no actor. It stands for XState's
 `ActorRef`.

 @see "Actor references become `actor_ref`", in the guide.
*/
using xactor::actor_ref;

/**
 Starts the actor, constructing it first if no parent did.

 The actor hands over what its initial macrostep returned and runs what it
 deferred, the starts of its active children first: XState's
 `actor.start()`. @ref actor_system::start sends it to a root, and a parent
 to each child it created once its macrostep has settled.

 @see "Guarantees", in the guide: guarantee A4.
*/
struct start_actor {};

/**
 Continues an actor the fuel of an earlier execution could not finish.

 @ref actor_system::resume sends it to each parked actor, in an execution of
 its own.

 @see "Guarantees", in the guide: guarantee A3.
*/
struct resume {};

/**
 A delayed sendTo whose timer fired: its sender forwards @ref event to
 @ref to.

 @note xactor releases a timer to the actor that armed it, as XState's
 scheduler keys a delayed event by its source; the target was resolved when
 the sendTo was.

 @see "Guarantees", in the guide: guarantee A7.
*/
struct relay {
    /** The actor the sendTo's target named when it was resolved. */
    actor_ref to{};

    /** The event to forward. */
    xstate::event event{};

    /** The id it was armed under, which its firing unregisters; none when it had none. */
    std::optional<std::string> send_id{};
};

/**
 A delayed event to the actor itself, armed under an id.

 Its firing unregisters the id, as XState's scheduler deletes its entry of
 `timerMap`, then the actor takes the event. A delayed event armed under no
 id comes back as the event itself.

 @see "Guarantees", in the guide: guarantee A7.
*/
struct delayed {
    /** The id it was armed under, which its firing unregisters. */
    std::string send_id{};

    /** The event the actor takes. */
    xstate::event event{};
};

/**
 The host's answer to a host actor: its output, or its error.

 @ref actor_system::resolve and @ref actor_system::reject send it.

 @see "Guarantees", in the guide: guarantee A10.
*/
struct host_answer {
    /** Whether the actor finishes done, with an output, rather than failed, with an error. */
    bool resolved = false;

    /**
     The output or the error; none for a promise settled with JavaScript's
     `undefined`.
    */
    std::optional<boost::json::value> value{};
};

/**
 Asks an actor to stop with its family, once it has handled what its parent
 sent it before.

 It is XState's deferred `stopChild`, which runs after the sends of the same
 macrostep. The actor finishes the macrostep it runs and the events that
 arrived before, ignores what arrives after, asks each of its own children
 to stop the same way, and stops, running no exit action. A child that has
 already ended ignores it.

 @see "Stopping a child", in the guide.
 @see "Guarantees", in the guide: guarantee A6.
*/
struct stop_actor {};

/**
 Constructs a machine child: it runs its initial macrostep and resolves its
 actions, then answers @ref constructed.

 It is XState's `createActor`, which its parent's `resolveSpawn` calls. The
 parent waits for the answer before it resolves its next action, so the
 children of the initial macrostep exist, and their systemIds are
 registered, when it does.

 @see "Guarantees", in the guide: guarantee A5.
*/
struct construct_actor {};

/** A child's answer to @ref construct_actor, once its own children are constructed too. */
struct constructed {};

/**
 Every message an actor of the layer receives.

 A host does not build them: the calls of @ref actor_system deliver them,
 and the logic classes send them to each other. Its alternatives are
 @ref start_actor, an @ref event to run as a macrostep, @ref resume,
 @ref relay, @ref host_answer, @ref stop_actor, @ref construct_actor,
 @ref constructed and @ref delayed.

 @note xactor names an alternative in its log by its index, so their order
 is part of a recorded run.

 @see "How the layer runs", in the guide.
*/
using actor_message = std::variant<start_actor, event, resume, relay, host_answer, stop_actor,
                                   construct_actor, constructed, delayed>;

/**
 The event an actor that ends sends its parent.

 Its type is `xstate.<kind>.actor.<id>`, and its payload holds `value`
 under `member` when there is one, then `actorId`, the `id`. The layer
 calls it with `"done"` and `"output"`, or `"error"` and `"error"`: XState's
 `createDoneActorEvent` and `createErrorActorEvent`.

 @param kind `"done"` or `"error"`.
 @param id The ending actor's id, as its parent knows it.
 @param member The payload member that holds `value`.
 @param value The output or the error; none for JavaScript's `undefined`,
 which leaves `member` out.
 @return The event.

 @see "Done and error events", in the guide.
 @see "Guarantees", in the guide: guarantee A6.
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
