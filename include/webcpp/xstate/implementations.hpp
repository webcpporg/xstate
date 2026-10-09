// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The implementations a caller registers by name, as XState's
 setup({ actions, actors, guards, delays }): the built-in actions a name
 stands for, the actors an invoke or a spawnChild names, the guards and the
 delays.

 @note An action name the registry does not hold is a custom action,
 returned to the caller; that is how a caller's own actions reach it.

 @see "Implementations", in the guide.
*/
#ifndef WEBCPP_XSTATE_IMPLEMENTATIONS_HPP
#define WEBCPP_XSTATE_IMPLEMENTATIONS_HPP

#include <webcpp/xstate/config.hpp>

#include <webcpp/xstate/errors.hpp>
#include <webcpp/xstate/event.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace webcpp::xstate {

class machine;

/**
 What an implementation is handed, built by xstate for the call.

 Every implementation but `implementations::context` is handed one. The
 references live during the call.

 @see "How guards and actions read the context", in the guide.
*/
struct action_args {
    /**
     The context as it is when the implementation runs, after the assigns
     that ran before it in the microstep.
    */
    const boost::json::value& context;

    /**
     The microstep's event.

     For the machine's output, computed by `implementations::outputs` under
     the root's id, it is the done event of the final state that completed
     the machine.
    */
    const xstate::event& event;

    /**
     The action's or the guard's `params` as the config wrote them.

     It is null when the config gave none, and for an input or an output.
    */
    const boost::json::value& params;
};

/**
 Returns the partial update an assign merges into the context.

 It is what XState's `assign` takes. A failure fails the macrostep that
 called it, which settles with the snapshot it began from, its status
 @ref status::error and the failure as its `error`.

 @see "Updating context with assign", in the guide.
*/
using assigner = std::function<result<boost::json::object>(const action_args&)>;

/**
 Returns the event a raise, a sendTo, a sendParent or an emit sends.

 A failure fails the macrostep that called it, as an @ref assigner's does.

 @see "Action objects and params", in the guide.
*/
using event_maker = std::function<result<event>(const action_args&)>;

/**
 Returns a value, or none for JavaScript's `undefined`, which leaves the
 member out.

 It computes the value a log logs, the input a child starts with, an
 invoke's input (`implementations::inputs`) and a state's or the machine's
 output (`implementations::outputs`). A failure fails the macrostep that
 called it, as an @ref assigner's does.

 @see "When a value is undefined", in the guide.
*/
using value_maker = std::function<result<std::optional<boost::json::value>>(const action_args&)>;

/**
 Decides whether a transition is taken.

 It ports a guard function of XState's `setup({ guards })`. A failure fails
 the macrostep that evaluated it, as an @ref assigner's does, and a call to
 @ref can too.

 @see "Guards", in the guide.
*/
using predicate = std::function<result<bool>(const action_args&)>;

/**
 Returns the initial context from the input the machine was started with.

 It ports XState's `context: ({ input }) => ...`. A failure, or a value that
 is not an object (@ref errc::implementation_failed), fails the initial
 macrostep.

 @see "Initial context from the input", in the guide.
*/
using context_maker = std::function<result<boost::json::value>(const boost::json::value& input)>;

/**
 Returns a delay, in milliseconds.

 It ports a delay function of XState's `setup({ delays })`. A failure fails
 the macrostep that called it, as an @ref assigner's does.

 @see "Delayed transitions", in the guide.
*/
using delay_maker = std::function<result<std::uint64_t>(const action_args&)>;

/**
 Computes a spawned child's id.

 It ports the `id` function of XState's `spawnChild`. A failure fails the
 macrostep that called it, as an @ref assigner's does.

 @see "Input to a spawned actor", in the guide.
*/
using id_maker = std::function<result<std::string>(const action_args&)>;

/**
 A delay as an action names it: a number of milliseconds, or the name of a
 delay of the registry.

 A name is the name of an entry of `implementations::delays`, which
 computes the delay when the action resolves. A name the registry does not
 hold is no delay, as in XState: the raise is queued at once, and the send
 is returned without a `delay`. An `after` key names a delay the same way,
 but @ref create_machine refuses one naming no registered delay, with
 @ref errc::unknown_delay.

 @see "Delayed raise, ids and cancel", in the guide.
*/
using delay_ref = std::variant<std::uint64_t, std::string>;

/**
 An assign, which merges a partial update into the context.

 It ports XState's `assign(assigner)`. The update is merged into the context
 as `Object.assign` merges it: each member replaces the context's member of
 that name, and the others stay. An assign changes the context and is never
 returned to the caller. The microstep fails with
 @ref errc::implementation_failed when the context is not an object, or
 with the assigner's own failure.

 @see "Assign", in the guide.
*/
struct assign_action {
    /** Computes the partial update. */
    assigner assignment{};
};

/**
 A raise, which sends an event to the machine itself.

 It ports XState's `raise(event, { id, delay })`. Without a delay, the event
 goes to the macrostep's internal queue and runs in a later microstep of the
 same macrostep; with one, it is only returned, for the caller's clock.
 Either way it is returned as `xstate.raise`, its params holding `event`,
 flat, `id` when it has one, and `delay`, in milliseconds, when it is
 delayed.

 @see "Raise", in the guide.
 @see "Delayed raise, ids and cancel", in the guide.
*/
struct raise_action {
    /** Computes the event to raise. */
    event_maker event{};

    /** The id a cancel names a delayed raise by. */
    std::optional<std::string> id{};

    /** The delay, none for a raise queued at once. */
    std::optional<delay_ref> delay{};
};

/**
 A log of a value, with an optional label.

 It ports XState's `log(value, label)`. It is returned as `xstate.log`, its
 params holding `value`, what the value maker returned, and `label` when it
 has one. The actor layer hands it to the listener of
 `actor_system::on_log`.

 @note Without a value maker the value is an object of the context and the
 event, as XState logs `{context, event}`; a value maker that returns none,
 JavaScript's `undefined`, leaves `value` out.

 @see "Log", in the guide.
*/
struct log_action {
    /** Computes the value to log; without one, the context and the event. */
    std::optional<value_maker> value{};

    /** The label, written with the value. */
    std::optional<std::string> label{};
};

/**
 A send to the machine's parent actor.

 It ports XState's `sendParent(event, { id, delay })`: a
 @ref send_to_action to `#_parent`, returned as `xstate.sendTo` to that
 target. It resolves for a machine without a parent too, where XState's pure
 functions throw, their inert actor having none; the actor layer fails a
 root actor that sends to its parent, with @ref errc::unknown_target.

 @see "Sending to other actors", in the guide.
*/
struct send_parent_action {
    /** Computes the event to send. */
    event_maker event{};

    /** The id a cancel names a delayed send by. */
    std::optional<std::string> id{};

    /** The delay, none for a send at once. */
    std::optional<delay_ref> delay{};
};

/**
 A cancel of a delayed raise or send.

 It ports XState's `cancel(id)`, returned as `xstate.cancel`, its params
 holding `sendId`, the id of the delayed raise or send to cancel. A state
 with `after` returns one of its own when it is left.

 @see "Delayed raise, ids and cancel", in the guide.
*/
struct cancel_action {
    /** The id of the delayed raise or send to cancel. */
    std::string id{};
};

/**
 A spawn of a child actor that lives until it is stopped or the machine is
 done.

 It ports XState's `spawnChild(src, { id, systemId, input })`. Resolving it
 adds the child, `(id, src)`, to the snapshot's `children`, a child already
 there under that id taking the new `src`, and returns `xstate.spawnChild`,
 its params holding `id`, `src`, and `input` and `systemId` when there are
 any. The machine core runs no child: its caller does, or the actor layer.
 @ref create_machine refuses, with @ref errc::unknown_actor, a spawnChild
 the config names whose `src` is not an actor of the implementations.

 @note XState keys a child spawned without an id "undefined", and a
 stopChild then leaves it there; an id left out here is "", and the child is
 keyed under it.

 @see "Spawning actors", in the guide.
 @see "A spawned child without an id", in the guide.
*/
struct spawn_child_action {
    /** The name of an entry of `implementations::actors`. */
    std::string src{};

    /** The child's id: fixed, or computed when the action resolves. */
    std::variant<std::string, id_maker> id{};

    /** The systemId the child is registered under in its actor system. */
    std::optional<std::string> system_id{};

    /** Computes the child's input; without one the input is undefined. */
    std::optional<value_maker> input{};
};

/**
 A stop of a child actor.

 It ports XState's `stopChild(id)`. It removes the child `id` from the
 snapshot's `children` and is returned as `xstate.stopChild`, its params
 holding `id`, or null when the snapshot held no such child. The actor
 layer stops the child with its family; a stop runs no exit action.

 @see "Stopping a child", in the guide.
*/
struct stop_child_action {
    /** The id of the child to stop. */
    std::string id{};
};

/**
 A send of an event to another actor, named by a target string.

 It ports XState's `sendTo(target, event, { id, delay })`. The target is a
 string as XState reads one: a child's id, or `#_` followed by it;
 `#_parent`, the parent; `#_internal`, the actor itself; or `#system:`
 followed by a systemId, the actor registered under it. It is returned as
 `xstate.sendTo`, its params holding `targetId`, `event`, flat, and `id`
 and `delay` as a raise's. The machine core delivers nothing: its caller
 does, or the actor layer.

 The microstep fails with @ref errc::unknown_target when the target names a
 child the snapshot does not hold, unless an entry or initial action names,
 by its bare id, an invoke of the state being entered: that sendTo carries
 @ref action::bound_at. `#_parent`, `#_internal` and `#system:` targets are
 always accepted here; the actor layer resolves them.

 @note XState sends to a systemId through a function of the system,
 `({ system }) => system.get(id)`, which JSON cannot hold.

 @see "Sending to other actors", in the guide.
*/
struct send_to_action {
    /** The actor to send to, as the target string names it. */
    std::string target{};

    /** Computes the event to send. */
    event_maker event{};

    /** The id a cancel names a delayed send by. */
    std::optional<std::string> id{};

    /** The delay, none for a send at once. */
    std::optional<delay_ref> delay{};
};

/**
 A send of the microstep's event to another actor.

 It ports XState's `forwardTo(target, { id, delay })`: a
 @ref send_to_action of the microstep's event, returned as `xstate.sendTo`
 with @ref action::forwarded set. The microstep fails with
 @ref errc::unknown_target for an empty target, as XState's development
 build refuses every falsy one, and the actor layer fails the actor when a
 `#system:` target names no actor, where a sendTo goes to the actor itself.

 @see "Sending to other actors", in the guide.
*/
struct forward_to_action {
    /** The actor to forward to, as the target string names it. */
    std::string target{};

    /** The id a cancel names a delayed send by. */
    std::optional<std::string> id{};

    /** The delay, none for a send at once. */
    std::optional<delay_ref> delay{};
};

/**
 An event emitted to the listeners of the actor, not to a machine.

 It ports XState's `emit(event)`. It is returned as `xstate.emit`, its
 params holding `event`, flat; the actor layer hands it to the inspector and
 to the listeners of `actor_system::on_emitted`.

 @see "Emitting events", in the guide.
*/
struct emit_action {
    /** Computes the event to emit. */
    event_maker event{};
};

/**
 The built-in action an entry of `implementations::actions` stands for.

 A name the config gives and the map does not hold is a custom action,
 returned to the caller as it was named.

 @see "Built-in and custom actions", in the guide.
*/
using action_implementation =
    std::variant<assign_action, raise_action, log_action, send_parent_action, cancel_action,
                 spawn_child_action, stop_child_action, send_to_action, forward_to_action,
                 emit_action>;

/**
 An actor whose work is done outside the machines, by the host.

 It stands for XState's `fromPromise`: invoking one asks the host, which
 later resolves it with an output or rejects it with an error, through
 `actor_system::resolve` and `actor_system::reject`. The machine core only
 names it; the actor layer runs it.

 @see "Host actors", in the guide.
 @see "Promises become host actors", in the guide.
*/
struct host_actor {};

/**
 A machine run as a child actor, with that machine's own implementations.

 It is what XState's `setup({ actors })` takes for a machine. The machine
 core only names it; the actor layer runs it.

 @note A machine holds its implementations, which may hold this, so this
 holds the machine through a pointer; machine.hpp defines the constructor.

 @see "Invoking actors", in the guide.
*/
struct machine_actor {
    /**
     Makes the actor of a machine.

     @param run The machine the child runs, shared with every copy of this.
    */
    explicit machine_actor(machine run);

    /** The machine the child runs. */
    std::shared_ptr<const machine> logic;
};

/**
 What an invoke's or a spawnChild's `src` names, registered in
 `implementations::actors`.

 It is what XState's `setup({ actors })` registers: a machine, or the work
 of the host.

 @see "Invoking actors", in the guide.
*/
using actor_implementation = std::variant<machine_actor, host_actor>;

/**
 The implementations a machine is created with, each registered by the name
 its config gives it.

 It ports XState's `setup({ actions, actors, guards, delays })`. A JSON
 config cannot hold a function, so it names each implementation, and an
 `implementations` maps the names to C++ functions; @ref create_machine
 takes it with the config. An empty one, `{}`, makes every action of the
 config a custom action, and holds no guard, delay or actor.

 @ref create_machine refuses, with @ref errc::invalid_config,
 implementations that hold an empty `std::function`, which a program
 without exceptions could not call; an entry of @ref inputs that names no
 invoke, or one whose config has an `input`; and an entry of @ref outputs
 that names neither a final state nor the root, or one whose config has an
 `output`.

 @see "Implementations", in the guide.
 @see "Functions become implementations registered by name", in the guide.
*/
struct implementations {
    /**
     Computes the initial context from the machine's input, in place of the
     config's `context`.

     It ports XState's `context: ({ input }) => ...`. A failure, or a value
     that is not an object (@ref errc::implementation_failed), fails the
     initial macrostep.
    */
    std::optional<context_maker> context{};

    /** The built-in actions, by action name; any other name is a custom action. */
    std::map<std::string, action_implementation, std::less<>> actions{};

    /** What an invoke or a spawnChild runs, by its `src`. */
    std::map<std::string, actor_implementation, std::less<>> actors{};

    /** The predicates a transition's `guard` names, by guard name. */
    std::map<std::string, predicate, std::less<>> guards{};

    /** The delays an `after` key or an action names, by delay name. */
    std::map<std::string, delay_maker, std::less<>> delays{};

    /**
     The input an invoke's child is given, by invoke id, in place of the
     config's `input`.

     It ports XState's invoke `input` function, which JSON cannot hold. The
     input is computed when the invoking state is entered, after the state's
     entry actions.
    */
    std::map<std::string, value_maker, std::less<>> inputs{};

    /**
     The output a final state is done with, by state id, or, by the root's
     id, the machine's, in place of the config's `output`.

     It ports XState's `output` function. A final state's output is computed
     from the context and the event that entered it; the machine's, from the
     done event of the final state that completed it.
    */
    std::map<std::string, value_maker, std::less<>> outputs{};
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_IMPLEMENTATIONS_HPP
