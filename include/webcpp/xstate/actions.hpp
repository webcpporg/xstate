// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The actions a microstep resolves: what each changes in the snapshot, and
 what is returned to the caller to execute (stateUtils.ts
 resolveAndExecuteActionsWithContext; actions/assign.ts, raise.ts, log.ts,
 cancel.ts, send.ts, spawnChild.ts, stopChild.ts, emit.ts).

 @note An assign only changes the context and is never returned; every other
 built-in is returned with its resolved params, and an action the registry
 does not hold is returned as it was named.

 @see "Actions", in the guide.
*/
#ifndef WEBCPP_XSTATE_ACTIONS_HPP
#define WEBCPP_XSTATE_ACTIONS_HPP

#include <webcpp/xstate/config.hpp>

#include <webcpp/xstate/errors.hpp>
#include <webcpp/xstate/event.hpp>
#include <webcpp/xstate/implementations.hpp>
#include <webcpp/xstate/machine.hpp>
#include <webcpp/xstate/snapshot.hpp>
#include <webcpp/xstate/state_node.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace webcpp::xstate {

/**
 An action returned to the caller to execute.

 It ports XState's `ExecutableActionObject`. A custom action (@ref builtin
 false) has the `type` the config named and the `params` it gave, null when
 it gave none. A built-in one was resolved by xstate, and `params` holds
 what it resolved: an `event` in XState's flat form, a `delay` in
 milliseconds.

 Each built-in type is returned for these actions, with these params:

 - `xstate.raise`, for a @ref raise_action and on entering a state with
   `after`, its params `event`, `id` when it has one and `delay` when
   delayed;
 - `xstate.cancel`, for a @ref cancel_action and on leaving a state with
   `after`, its params `sendId`;
 - `xstate.log`, for a @ref log_action, its params `value` unless undefined
   and `label` when it has one;
 - `xstate.sendTo`, for a @ref send_to_action, a @ref send_parent_action
   and a @ref forward_to_action, its params `targetId`, `event`, `id` when
   it has one and `delay` when delayed;
 - `xstate.emit`, for an @ref emit_action, its params `event`;
 - `xstate.spawnChild`, for a @ref spawn_child_action and on entering a
   state that invokes, its params `id`, `src`, and `input` and `systemId`
   when there are any;
 - `xstate.stopChild`, for a @ref stop_child_action, on leaving a state
   that invokes, and for each child left when a macrostep settles not
   active, its params `id`, or null when the child was not there.

 An assign is never returned.

 @note @ref builtin tells the two kinds apart, not the type: a config may
 name an action `xstate.sendTo` and give it no implementation, and it is
 returned as a custom action all the same, which
 @ref simulated_clock::apply ignores.

 @see "Built-in and custom actions", in the guide.
 @see "Custom actions", in the guide.
*/
struct action {
    /** The action's type: the name the config gave, or a built-in's `xstate.` name. */
    std::string type{};

    /** The resolved params of a built-in, or the params the config gave, null when none. */
    boost::json::value params{};

    /**
     Whether xstate resolved the action, as a raise, a sendTo or a
     spawnChild does, as opposed to a custom action returned as the config
     named it.
    */
    bool builtin = false;

    /**
     Where XState binds a sendTo that names, by its bare id, an invoke of
     the state being entered.

     It counts how many of its microstep's actions come before the point
     where XState binds the sendTo to the child that id names: the end of
     that state's entry actions, spawns and initial actions (XState's
     `retryResolveSendTo`). When no child has that id at that point, the
     actor layer fails the actor with @ref errc::unknown_target where XState
     throws.

     @see "A `sendTo` whose child is gone", in the guide.
    */
    std::optional<std::size_t> bound_at{};

    /**
     Whether the action is a forwardTo's sendTo.

     XState's development build refuses to resolve a forwardTo to no actor,
     where a sendTo goes to the actor itself; the actor layer reads it.
    */
    bool forwarded = false;
};

namespace detail {

/** What resolving a list of actions changes, outside the snapshot. */
struct resolution {
    /** The macrostep's queue, to which a raise without a delay goes. */
    std::deque<event>& queue;

    /** The actions returned to the caller, in order. */
    std::vector<action>& returned;

    /** Set when the snapshot changed, which re-enables eventless transitions. */
    bool& changed;
};

/**
 A delay as a number of milliseconds, or nothing when a named delay is not
 registered, which XState treats as no delay.
*/
inline result<std::optional<std::uint64_t>> resolve_delay(const machine& owner,
                                                          const delay_ref& delay,
                                                          const action_args& args) {
    if (const std::uint64_t* milliseconds = std::get_if<std::uint64_t>(&delay)) {
        return std::optional<std::uint64_t>(*milliseconds);
    }
    const auto found = owner.registry().delays.find(std::get<std::string>(delay));
    if (found == owner.registry().delays.end()) {
        return std::optional<std::uint64_t>();
    }
    const result<std::uint64_t> resolved = found->second(args);
    if (!resolved.has_value()) {
        return resolved.error();
    }
    return std::optional<std::uint64_t>(*resolved);
}

/**
 A raise: queued at once without a delay, only returned with one; XState's
 resolveRaise.
*/
inline result<void> resolve_raise(const machine& owner, const event& raised,
                                  const std::optional<std::string>& id,
                                  const std::optional<delay_ref>& delay, const action_args& args,
                                  const resolution& into) {
    std::optional<std::uint64_t> milliseconds;
    if (delay.has_value()) {
        const result<std::optional<std::uint64_t>> resolved = resolve_delay(owner, *delay, args);
        if (!resolved.has_value()) {
            return resolved.error();
        }
        milliseconds = *resolved;
    }
    boost::json::object params;
    params.emplace("event", to_json(raised));
    if (id.has_value()) {
        params.emplace("id", *id);
    }
    if (milliseconds.has_value()) {
        params.emplace("delay", *milliseconds);
    } else {
        into.queue.push_back(raised);
    }
    into.returned.push_back(
        action{.type = "xstate.raise", .params = std::move(params), .builtin = true});
    return {};
}

/**
 Whether a sendTo's target names an actor, as XState's resolveSendTo finds
 one: the parent, the machine itself and a systemId always, their caller
 resolving them; a child when the snapshot holds it, or, named by its bare
 id, when the state being entered invokes it (XState's deferredActorIds).
*/
inline bool names_an_actor(const snapshot& current, std::string_view target,
                           std::span<const invoke_definition> deferred) {
    if (target == "#_parent" || target == "#_internal" || target.starts_with("#system:")) {
        return true;
    }
    const auto held = [&current](std::string_view id) {
        return std::ranges::any_of(current.children,
                                   [id](const auto& child) { return child.first == id; });
    };
    if (target.starts_with("#_")) {
        return held(target.substr(2));
    }
    return held(target) || std::ranges::any_of(deferred, [target](const invoke_definition& one) {
               return one.id == target;
           });
}

/**
 A sendTo: always returned for the caller to deliver, its delay resolved,
 and failed when its target names no actor; XState's resolveSendTo.
*/
inline result<void> resolve_send_to(const machine& owner, const snapshot& current,
                                    std::string_view target, const event& sent,
                                    const std::optional<std::string>& id,
                                    const std::optional<delay_ref>& delay, const action_args& args,
                                    std::span<const invoke_definition> deferred,
                                    const resolution& into) {
    std::optional<std::uint64_t> milliseconds;
    if (delay.has_value()) {
        const result<std::optional<std::uint64_t>> resolved = resolve_delay(owner, *delay, args);
        if (!resolved.has_value()) {
            return resolved.error();
        }
        milliseconds = *resolved;
    }
    if (!names_an_actor(current, target, deferred)) {
        return failure<void>(errc::unknown_target);
    }
    boost::json::object params{{"targetId", target}, {"event", to_json(sent)}};
    if (id.has_value()) {
        params.emplace("id", *id);
    }
    if (milliseconds.has_value()) {
        params.emplace("delay", *milliseconds);
    }
    into.returned.push_back(
        action{.type = "xstate.sendTo", .params = std::move(params), .builtin = true});
    return {};
}

/**
 A spawn: the child added to the snapshot's children under its id and
 returned for the caller to start; XState's resolveSpawn.

 @note XState's params also hold the actor reference it creates; here the
 caller creates the child from `src`.
*/
inline void resolve_spawn(snapshot& current, std::string_view id, std::string_view src,
                          const std::optional<boost::json::value>& input,
                          const std::optional<std::string>& system_id, const resolution& into) {
    boost::json::object params{{"id", id}, {"src", src}};
    if (input.has_value()) {
        params.emplace("input", *input);
    }
    if (system_id.has_value()) {
        params.emplace("systemId", *system_id);
    }
    set_child(current, std::string(id), std::string(src));
    into.returned.push_back(
        action{.type = "xstate.spawnChild", .params = std::move(params), .builtin = true});
    into.changed = true;
}

/**
 A stop: the child removed from the snapshot's children and returned for
 the caller to stop, with no params when there is no such child; XState's
 resolveStop.
*/
inline void resolve_stop(snapshot& current, std::string_view id, const resolution& into) {
    boost::json::value params;
    if (remove_child(current, id)) {
        params = boost::json::object{{"id", id}};
    }
    into.returned.push_back(
        action{.type = "xstate.stopChild", .params = std::move(params), .builtin = true});
    into.changed = true;
}

/**
 An assign: the partial update its implementation returns, merged into the
 context as Object.assign merges it; XState's resolveAssign.
*/
inline result<void> resolve_assign(snapshot& current, const assign_action& assigned,
                                   const action_args& args, const resolution& into) {
    if (!current.context.is_object()) {
        return failure<void>(errc::implementation_failed);
    }
    const result<boost::json::object> update = assigned.assignment(args);
    if (!update.has_value()) {
        return update.error();
    }
    boost::json::object merged = current.context.get_object();
    for (const auto& member : *update) {
        merged[member.key()] = member.value();
    }
    current.context = std::move(merged);
    into.changed = true;
    return {};
}

/** A log: its value, {context, event} without one; XState's resolveLog. */
inline result<void> resolve_log(const log_action& logged, const action_args& args,
                                const resolution& into) {
    std::optional<boost::json::value> value;
    if (logged.value.has_value()) {
        result<std::optional<boost::json::value>> made = (*logged.value)(args);
        if (!made.has_value()) {
            return made.error();
        }
        value = std::move(*made);
    } else {
        value = boost::json::object{{"context", args.context}, {"event", to_json(args.event)}};
    }
    boost::json::object params;
    if (value.has_value()) {
        params.emplace("value", std::move(*value));
    }
    if (logged.label.has_value()) {
        params.emplace("label", *logged.label);
    }
    into.returned.push_back(
        action{.type = "xstate.log", .params = std::move(params), .builtin = true});
    return {};
}

/**
 Resolves a registered action as the built-in it stands for, one overload
 per alternative of action_implementation.
*/
struct registered_resolver {
    /** The machine whose implementations the action comes from. */
    const machine& owner;

    /** The snapshot the action changes. */
    snapshot& current;

    /** What the action's implementations are handed. */
    const action_args& args;

    /** The ids a sendTo may name before their child is spawned. */
    std::span<const invoke_definition> deferred{};

    /** What the action returns or queues. */
    const resolution& into;

    /** An assign: XState's resolveAssign. */
    result<void> operator()(const assign_action& assigned) const {
        return resolve_assign(current, assigned, args, into);
    }

    /** A raise, its event made first: XState's resolveRaise. */
    result<void> operator()(const raise_action& raised) const {
        const result<event> made = raised.event(args);
        if (!made.has_value()) {
            return made.error();
        }
        return resolve_raise(owner, *made, raised.id, raised.delay, args, into);
    }

    /** A log: XState's resolveLog. */
    result<void> operator()(const log_action& logged) const {
        return resolve_log(logged, args, into);
    }

    /** XState's sendParent is its sendTo to #_parent. */
    result<void> operator()(const send_parent_action& sent) const {
        const result<event> made = sent.event(args);
        if (!made.has_value()) {
            return made.error();
        }
        return resolve_send_to(owner, current, "#_parent", *made, sent.id, sent.delay, args, {},
                               into);
    }

    /** A cancel, returned with its id as `sendId`: XState's resolveCancel. */
    result<void> operator()(const cancel_action& cancelled) const {
        into.returned.push_back(action{
            .type = "xstate.cancel",
            .params = boost::json::object{{"sendId", cancelled.id}},
            .builtin = true,
        });
        return {};
    }

    /** A spawnChild, its id and input computed first. */
    result<void> operator()(const spawn_child_action& spawned) const {
        std::string id;
        if (const auto* fixed = std::get_if<std::string>(&spawned.id)) {
            id = *fixed;
        } else {
            result<std::string> made = std::get<id_maker>(spawned.id)(args);
            if (!made.has_value()) {
                return made.error();
            }
            id = std::move(*made);
        }
        std::optional<boost::json::value> input;
        if (spawned.input.has_value()) {
            result<std::optional<boost::json::value>> made = (*spawned.input)(args);
            if (!made.has_value()) {
                return made.error();
            }
            input = std::move(*made);
        }
        resolve_spawn(current, id, spawned.src, input, spawned.system_id, into);
        return {};
    }

    /** A stopChild: XState's resolveStop. */
    result<void> operator()(const stop_child_action& stopped) const {
        resolve_stop(current, stopped.id, into);
        return {};
    }

    /** A sendTo, its event made first: XState's resolveSendTo. */
    result<void> operator()(const send_to_action& sent) const {
        const result<event> made = sent.event(args);
        if (!made.has_value()) {
            return made.error();
        }
        return resolve_send_to(owner, current, sent.target, *made, sent.id, sent.delay, args,
                               deferred, into);
    }

    /** XState's forwardTo is its sendTo of the microstep's event. */
    result<void> operator()(const forward_to_action& forwarded) const {
        if (forwarded.target.empty()) {
            // XState's development build refuses every falsy target.
            return failure<void>(errc::unknown_target);
        }
        const result<void> resolved =
            resolve_send_to(owner, current, forwarded.target, args.event, forwarded.id,
                            forwarded.delay, args, deferred, into);
        if (resolved.has_value()) {
            into.returned.back().forwarded = true;
        }
        return resolved;
    }

    /** XState's resolveEmit. */
    result<void> operator()(const emit_action& emitted) const {
        const result<event> made = emitted.event(args);
        if (!made.has_value()) {
            return made.error();
        }
        into.returned.push_back(action{
            .type = "xstate.emit",
            .params = boost::json::object{{"event", to_json(*made)}},
            .builtin = true,
        });
        return {};
    }
};

/**
 Resolves `actions` in order against the snapshot as each leaves it; a
 sendTo may name a child of `deferred` before its spawn; XState's
 resolveActionsAndContext.
*/
inline result<void> resolve_actions(const machine& owner, snapshot& current, const event& happened,
                                    const std::vector<const action_ref*>& actions,
                                    const resolution& into,
                                    std::span<const invoke_definition> deferred = {}) {
    for (const action_ref* named : actions) {
        const boost::json::value context = current.context;
        const action_args args{.context = context, .event = happened, .params = named->params};
        if (named->kind == action_ref::kind::after_raise) {
            const result<void> raised =
                resolve_raise(owner, event{.type = named->type, .payload = {}}, named->type,
                              named->delay, args, into);
            if (!raised.has_value()) {
                return raised;
            }
            continue;
        }
        if (named->kind == action_ref::kind::after_cancel) {
            into.returned.push_back(action{
                .type = "xstate.cancel",
                .params = boost::json::object{{"sendId", named->type}},
                .builtin = true,
            });
            continue;
        }
        const auto found = owner.registry().actions.find(named->type);
        if (found == owner.registry().actions.end()) {
            into.returned.push_back(action{.type = named->type, .params = named->params});
            continue;
        }
        const registered_resolver resolver{
            .owner = owner,
            .current = current,
            .args = args,
            .deferred = deferred,
            .into = into,
        };
        if (const result<void> resolved = std::visit(resolver, found->second);
            !resolved.has_value()) {
            return resolved;
        }
    }
    return {};
}

}  // namespace detail

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_ACTIONS_HPP
