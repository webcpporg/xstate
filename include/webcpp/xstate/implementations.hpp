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

 Tip: an action name the registry does not hold is a custom action, returned
 to the caller; that is how a caller's own actions reach it.
*/
#ifndef WEBCPP_XSTATE_IMPLEMENTATIONS_HPP
#define WEBCPP_XSTATE_IMPLEMENTATIONS_HPP

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
 What an implementation is handed: the context as it is when it runs, the
 event of the microstep, and the action's or the guard's `params` (null when
 the config gave none).
*/
struct action_args {
    const boost::json::value& context;
    const xstate::event& event;
    const boost::json::value& params;
};

/** Returns the partial update an assign merges into the context. */
using assigner = std::function<result<boost::json::object>(const action_args&)>;

/** Returns the event a raise, a sendTo, a sendParent or an emit sends. */
using event_maker = std::function<result<event>(const action_args&)>;

/**
 Returns the value a log logs, the input a child starts with or a state's
 output; none for JavaScript's undefined, which leaves the member out.
*/
using value_maker = std::function<result<std::optional<boost::json::value>>(const action_args&)>;

/** Decides whether a transition is taken. */
using predicate = std::function<result<bool>(const action_args&)>;

/**
 Returns the initial context from the input the machine was started with;
 XState's context: ({ input }) => ....
*/
using context_maker = std::function<result<boost::json::value>(const boost::json::value& input)>;

/** Returns a delay, in milliseconds. */
using delay_maker = std::function<result<std::uint64_t>(const action_args&)>;

/** Computes a spawned child's id; XState's spawnChild id function. */
using id_maker = std::function<result<std::string>(const action_args&)>;

/**
 A delay as an action names it: a number of milliseconds, or the name of a
 delay of the registry.
*/
using delay_ref = std::variant<std::uint64_t, std::string>;

/** XState's assign(assigner). */
struct assign_action {
    assigner assignment{};
};

/** XState's raise(event, { id, delay }). */
struct raise_action {
    event_maker event{};
    std::optional<std::string> id{};
    std::optional<delay_ref> delay{};
};

/**
 XState's log(value, label).

 Tip: without a value, XState logs {context, event}.
*/
struct log_action {
    std::optional<value_maker> value{};
    std::optional<std::string> label{};
};

/** XState's sendParent(event, { id, delay }). */
struct send_parent_action {
    event_maker event{};
    std::optional<std::string> id{};
    std::optional<delay_ref> delay{};
};

/** XState's cancel(id). */
struct cancel_action {
    std::string id{};
};

/**
 XState's spawnChild(src, { id, systemId, input }): a child that lives until
 it is stopped or the machine is done.

 Tip: XState keys a child spawned without an id "undefined", and a
 stopChild then leaves it there; an id left out here is "", and the child is
 keyed under it.
*/
struct spawn_child_action {
    std::string src{};
    // Fixed, or computed when the action resolves.
    std::variant<std::string, id_maker> id{};
    std::optional<std::string> system_id{};
    // Computes the child's input; without one the input is undefined.
    std::optional<value_maker> input{};
};

/** XState's stopChild(id). */
struct stop_child_action {
    std::string id{};
};

/**
 XState's sendTo(target, event, { id, delay }). The target is a string as
 XState reads one: a child's id, "#_" and a child's id, "#_parent" or
 "#_internal"; or "#system:" and a systemId, the actor registered under it.

 Tip: XState sends to a systemId through a function of the system,
 ({ system }) => system.get(id), which JSON cannot hold.
*/
struct send_to_action {
    std::string target{};
    event_maker event{};
    std::optional<std::string> id{};
    std::optional<delay_ref> delay{};
};

/** XState's forwardTo(target, { id, delay }): a sendTo of the microstep's event. */
struct forward_to_action {
    std::string target{};
    std::optional<std::string> id{};
    std::optional<delay_ref> delay{};
};

/** XState's emit(event): an event for the listeners of the actor. */
struct emit_action {
    event_maker event{};
};

using action_implementation =
    std::variant<assign_action, raise_action, log_action, send_parent_action, cancel_action,
                 spawn_child_action, stop_child_action, send_to_action, forward_to_action,
                 emit_action>;

/**
 An actor whose work is done outside the machines, in place of XState's
 fromPromise: invoking one asks the host, which later resolves it with an
 output or rejects it with an error (actors/actor_system.hpp).
*/
struct host_actor {};

/**
 A machine run as a child actor, as XState's setup({ actors }) takes one.

 Tip: a machine holds its implementations, so this holds the machine
 through a pointer; machine.hpp defines the constructor.
*/
struct machine_actor {
    explicit machine_actor(machine run);

    std::shared_ptr<const machine> logic;
};

using actor_implementation = std::variant<machine_actor, host_actor>;

struct implementations {
    // When set, the initial context is what it returns for the input, in
    // place of the config's `context`.
    std::optional<context_maker> context{};
    std::map<std::string, action_implementation, std::less<>> actions{};
    // By src: what an invoke or a spawnChild runs.
    std::map<std::string, actor_implementation, std::less<>> actors{};
    std::map<std::string, predicate, std::less<>> guards{};
    std::map<std::string, delay_maker, std::less<>> delays{};
    // By invoke id: the input an invoke's child is given, computed when its
    // state is entered, in place of the config's `input`; XState's input
    // function, which JSON cannot hold.
    std::map<std::string, value_maker, std::less<>> inputs{};
    // By state id: the output a final state is done with, or, by the root's
    // id, the machine's, in place of the config's `output`; XState's output
    // function.
    std::map<std::string, value_maker, std::less<>> outputs{};
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_IMPLEMENTATIONS_HPP
