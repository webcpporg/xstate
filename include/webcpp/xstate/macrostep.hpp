// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The macrostep cursor, xstate's one primitive: each next() runs exactly one
 microstep and says whether it settled the macrostep
 (doc: #xstate-invariant-6 to #xstate-invariant-9). XState's macrostep loop,
 one iteration a call (stateUtils.ts macrostep, StateMachine.ts
 getInitialSnapshot).

 @note After each microstep the cursor already selects the eventless
 transitions XState's next iteration would take, so `settled` is known on
 the microstep that closes the macrostep and the next call does not select
 them again.

 @see "The macrostep cursor", in the guide.
*/
#ifndef WEBCPP_XSTATE_MACROSTEP_HPP
#define WEBCPP_XSTATE_MACROSTEP_HPP

#include <webcpp/xstate/config.hpp>

#include <webcpp/xstate/actions.hpp>
#include <webcpp/xstate/errors.hpp>
#include <webcpp/xstate/event.hpp>
#include <webcpp/xstate/machine.hpp>
#include <webcpp/xstate/microstep.hpp>
#include <webcpp/xstate/snapshot.hpp>

#include <cstddef>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

namespace webcpp::xstate {

/**
 One microstep: the event it ran for, the transitions it took, the actions
 it returned and the snapshot it left.

 It is what XState's `getMicrosteps` and `getInitialMicrosteps` list for
 each microstep.

 @see "Microsteps", in the guide.
*/
struct microstep {
    /**
     The event the microstep ran for: the macrostep's, a raised one, or the
     last one again for an eventless transition.
    */
    xstate::event event{};

    /**
     The transitions the microstep took, as pointers into the machine.

     It is none for the initial microstep, which XState does not report, and
     for a microstep that failed, and an empty list for an event no
     transition takes.

     @note A transition belongs to the machine and lives as long as any copy
     of it does.
    */
    std::optional<std::vector<const transition_definition*>> transitions{};

    /** The actions the microstep returned, in order. */
    std::vector<action> actions{};

    /** The snapshot the microstep left. */
    xstate::snapshot snapshot{};
};

namespace detail {

/**
 The microstep of an actor's error event no transition handles: the
 snapshot it began from, failed with the actor's error; XState's macrostep,
 for an event isErrorActorEvent accepts.
*/
inline microstep_outcome actor_failure(const snapshot& current, const event& happened) {
    microstep_outcome failed{.next = current, .changed = true};
    failed.next.status = status::error;
    failed.next.error = make_error_code(errc::actor_failed);
    if (const boost::json::value* error = happened.payload.if_contains("error")) {
        failed.next.error_value = *error;
    }
    return failed;
}

}  // namespace detail

/**
 What one call of @ref macrostep::next did: its microstep, and whether it
 settled the macrostep.

 A microstep that fails reports no actions, as XState's pure functions
 throw; @ref resolved_before_failure holds those it resolved before the
 failure, and the macrostep's result holds none of them.

 @see "One microstep at a time", in the guide.
*/
struct progress {
    /** The microstep the call ran. */
    microstep step{};

    /**
     Whether the microstep settled the macrostep.

     It is true on the microstep after which XState's macrostep loop would
     stop, when no eventless transition is enabled and no raised event is
     queued, or the status is no longer @ref status::active; and on no
     other.

     @see "Guarantees", in the guide: guarantee 7.
    */
    bool settled = false;

    /**
     The actions a microstep that failed resolved before the failure.

     Of those, a running actor of XState has run the custom actions and
     logs, and none when the macrostep is an initial one. The actor layer
     reads it.
    */
    std::vector<action> resolved_before_failure{};
};

/**
 A settled macrostep: its stable snapshot, every action its microsteps
 returned, and how many microsteps ran.

 When the macrostep fails, @ref snapshot is the snapshot it began from, with
 the status @ref status::error. @ref actions still holds the actions of the
 microsteps that completed before the failure, including the microstep
 after which an eventless guard failed, and @ref microsteps counts them. Of
 those actions, a running actor of XState has executed the custom actions
 and logs, which it runs as each microstep is resolved, and none of the
 built-in ones (a sendTo, an emit, a spawnChild, the stopChild of a started
 child, a delayed raise, a cancel), whose effects it defers until the
 macrostep has settled and drops when it throws. XState's actor executes
 none of an initial macrostep that fails, since it runs an actor's initial
 actions when the actor starts, and the start of an actor whose initial
 snapshot failed runs none. The actor layer does the same, and a caller
 that stands for XState's actor executes the same ones.

 @see "When an implementation fails", in the guide.
*/
struct macrostep_result {
    /** The stable snapshot the macrostep settled in. */
    xstate::snapshot snapshot{};

    /**
     Every action the microsteps returned, in order.

     When the macrostep settles not active, they are followed by an
     `xstate.stopChild` for each child the snapshot still lists, which no
     microstep returns (XState's `stopChildren`). A macrostep that fails, or
     that an unhandled `xstate.error.actor` event settles, returns no such
     stop.
    */
    std::vector<action> actions{};

    /** How many microsteps ran, a failing one excluded. */
    std::size_t microsteps = 0;
};

/**
 The cursor of one macrostep, which runs one microstep a call.

 It ports XState's macrostep loop, one iteration a call. XState runs a
 macrostep to its end; the cursor lets its caller decide how much work an
 event may cost, one unit of fuel a call, and stop and resume where it
 likes. @ref begin_initial and @ref begin make one; it has no public
 constructor. A cursor is a value: it holds a copy of its machine, a handle,
 and of its snapshots and queue, so a copy resumes exactly where the
 original stopped, apart from it. xstate never stops a macrostep on its
 own: an `always` cycle never settles, and the caller's fuel is the only
 bound.

 @see "The macrostep cursor", in the guide.
 @see "Fuel", in the guide.
 @see "Guarantees", in the guide: guarantee 8.
*/
class macrostep {
public:
    /**
     Whether the macrostep has settled.

     @return `true` once it has; @ref next then runs nothing.
    */
    [[nodiscard]] bool done() const noexcept { return settled_; }

    /**
     The snapshot as the last microstep left it.

     @return The current snapshot, which between two microsteps is not a
     stable state.
    */
    [[nodiscard]] const snapshot& current() const noexcept { return current_; }

    /**
     The snapshot the macrostep began from.

     @return The snapshot before the macrostep's first microstep.
    */
    [[nodiscard]] const snapshot& began_from() const noexcept { return start_; }

    /**
     The settled macrostep.

     @return The result, meaningful once @ref done.
    */
    [[nodiscard]] const macrostep_result& result() const noexcept { return result_; }

    /**
     Runs exactly one microstep.

     It runs the initial microstep, the external event's, or one iteration
     of XState's loop, an enabled eventless transition first and a raised
     event otherwise. Once the macrostep has settled it runs nothing, and
     returns a settled @ref progress with the current snapshot, no list of
     transitions and no actions.

     It returns no error: a failure settles the macrostep with the snapshot
     it began from, its status @ref status::error and the failure as its
     `error`. The failure is an implementation's, with its own code, or one
     of these:

     - @ref errc::unknown_target when a sendTo names no actor, or a
       microstep needs the default of a history state that is the machine's
       root;
     - @ref errc::implementation_failed when an `assign` runs on a context
       that is not an object, for example the context of a config whose
       `context` is `5`;
     - @ref errc::invalid_event when the external event's type is `*`;
     - @ref errc::unknown_state when the snapshot's value names no state of
       the machine, or a `stateIn` guard's `#id` names none;
     - the context function's error, or @ref errc::implementation_failed
       when its value is not an object, on the first call of an initial
       macrostep.

     When the failure comes from a guard of an eventless transition, which
     the cursor evaluates after a microstep to know whether it settled, the
     call reports the microstep it ran, with its transitions and actions,
     and the failed snapshot. An external event whose type starts with
     `xstate.error.actor` and that no transition takes is no failure: it
     settles in one microstep with an empty list of transitions, the
     snapshot it began from, its status @ref status::error, its error
     @ref errc::actor_failed and its `error_value` the event's member
     `error`.

     @return What the call did.

     @note XState's actor, too, keeps the snapshot from before a macrostep
     that threw.

     @see "When an implementation fails", in the guide.
    */
    progress next() {
        if (refused_.has_value()) {
            const boost::system::error_code refusal = *refused_;
            refused_.reset();
            return fail(refusal);
        }
        if (settled_) {
            return progress{
                .step =
                    {
                        .event = event_,
                        .transitions = std::nullopt,
                        .actions = {},
                        .snapshot = current_,
                    },
                .settled = true,
            };
        }
        std::optional<detail::transition_list> taken;
        detail::microstep_outcome outcome = run_one(taken);
        if (outcome.failure.has_value()) {
            progress failed = fail(*outcome.failure);
            failed.resolved_before_failure = std::move(outcome.actions);
            return failed;
        }
        current_ = std::move(outcome.next);
        result_.actions.insert(result_.actions.end(), outcome.actions.begin(),
                               outcome.actions.end());
        ++result_.microsteps;
        progress made{
            .step =
                {
                    .event = event_,
                    .transitions = std::move(taken),
                    .actions = std::move(outcome.actions),
                    .snapshot = current_,
                },
        };
        if (const xstate::result<void> looked = look_ahead(); !looked.has_value()) {
            progress failed = fail(looked.error());
            failed.step.transitions = std::move(made.step.transitions);
            failed.step.actions = std::move(made.step.actions);
            return failed;
        }
        made.settled = settled_;
        return made;
    }

private:
    enum class phase { initial, external, loop };

    macrostep(machine owner, snapshot start, event happened, phase first)
        : machine_(std::move(owner)),
          start_(start),
          current_(std::move(start)),
          event_(std::move(happened)),
          phase_(first) {}

    friend macrostep begin_initial(const machine& owner, const boost::json::value& input);
    friend macrostep begin(const machine& owner, snapshot from, event happened);

    /** Runs this call's microstep, recording the transitions it reports. */
    detail::microstep_outcome run_one(std::optional<detail::transition_list>& taken) {
        if (phase_ == phase::initial) {
            phase_ = phase::loop;
            const transition_definition entering{
                .source = 0,
                .target = detail::initial_state_nodes(machine_, 0).ordered(),
                .reenter = true,
            };
            return detail::run_microstep(machine_, {&entering}, current_, event_, true, queue_);
        }
        if (phase_ == phase::external) {
            phase_ = phase::loop;
            if (event_.type == wildcard) {
                // XState's development build refuses it here, and only here:
                // a raised one reaches the wildcard transitions.
                return detail::ended(detail::microstep_outcome{.next = current_},
                                     make_error_code(errc::invalid_event));
            }
            xstate::result<detail::transition_list> selected =
                detail::select_transitions(machine_, event_, current_);
            if (!selected.has_value()) {
                return detail::ended(detail::microstep_outcome{.next = current_}, selected.error());
            }
            taken = *selected;
            // XState's isErrorActorEvent reads a prefix, not a segment.
            if (selected->empty() && event_.type.starts_with("xstate.error.actor")) {
                error_rule_ = true;
                return detail::actor_failure(current_, event_);
            }
            return detail::run_microstep(machine_, *selected, current_, event_, false, queue_);
        }
        detail::transition_list selected;
        const bool eventless = enabled_.has_value() && !enabled_->empty();
        if (eventless) {
            selected = std::move(*enabled_);
        } else {
            event_ = std::move(queue_.front());
            queue_.pop_front();
            xstate::result<detail::transition_list> chosen =
                detail::select_transitions(machine_, event_, current_);
            if (!chosen.has_value()) {
                return detail::ended(detail::microstep_outcome{.next = current_}, chosen.error());
            }
            selected = std::move(*chosen);
        }
        enabled_.reset();
        taken = selected;
        detail::microstep_outcome outcome =
            detail::run_microstep(machine_, selected, current_, event_, false, queue_);
        if (!outcome.failure.has_value()) {
            select_eventless_ = !eventless || outcome.changed;
        }
        return outcome;
    }

    /**
     Decides whether XState's loop would stop now, selecting the eventless
     transitions its next iteration would take; XState's loop condition.
    */
    xstate::result<void> look_ahead() {
        if (current_.status != status::active) {
            if (!error_rule_) {
                stop_children();
            }
            settle();
            return {};
        }
        detail::transition_list enabled;
        if (select_eventless_) {
            xstate::result<detail::transition_list> selected =
                detail::select_eventless(machine_, current_, event_);
            if (!selected.has_value()) {
                return selected.error();
            }
            enabled = std::move(*selected);
        }
        if (enabled.empty() && queue_.empty()) {
            settle();
            return {};
        }
        enabled_ = std::move(enabled);
        return {};
    }

    void settle() {
        settled_ = true;
        result_.snapshot = current_;
    }

    /**
     Returns a stop for every child a machine that is not active has left,
     after the microsteps' actions and in none of them; the snapshot keeps
     its children. XState's stopChildren, whose snapshot its macrostep drops.
    */
    void stop_children() {
        for (const auto& [id, src] : current_.children) {
            result_.actions.push_back(action{
                .type = "xstate.stopChild",
                .params = boost::json::object{{"id", id}},
                .builtin = true,
            });
        }
    }

    progress fail(boost::system::error_code error) {
        current_ = start_;
        current_.status = status::error;
        current_.error = error;
        settle();
        return progress{
            .step =
                {
                    .event = event_,
                    .transitions = std::nullopt,
                    .actions = {},
                    .snapshot = current_,
                },
            .settled = true,
        };
    }

    machine machine_;
    snapshot start_;
    snapshot current_;
    event event_;
    phase phase_;
    std::deque<event> queue_;
    std::optional<detail::transition_list> enabled_;
    bool select_eventless_ = true;
    bool settled_ = false;
    // Whether an unhandled error event settled it, which stops no child.
    bool error_rule_ = false;
    // Why the initial context could not be computed, reported by the first
    // next(); XState's getInitialSnapshot catches it the same way.
    std::optional<boost::system::error_code> refused_;
    macrostep_result result_;
};

/**
 Makes the cursor of a machine's initial macrostep.

 It ports XState's `getInitialSnapshot` and `_getPreInitialState`, one
 microstep a call: the initial microstep, which enters the initial states,
 then XState's loop for the event `xstate.init`, which carries `input` as
 its member `input` when it is not null. The initial context is
 `owner.context()`, or, when `implementations::context` is set, what it
 returns for `input`. The cursor's @ref macrostep::began_from is the
 snapshot before the initial microstep, whose `nodes` hold the root alone.

 @note A context that cannot be computed, because the context function
 fails or returns a value that is not an object
 (@ref errc::implementation_failed), fails the macrostep on its first
 @ref macrostep::next, with the snapshot before it, as XState's
 `getInitialSnapshot` catches the error into the snapshot it returns.

 @param owner The machine.
 @param input The machine's input, null for none.
 @return The cursor, which has run no microstep yet.

 @see "The initial macrostep", in the guide.
 @see "A machine's input", in the guide.
*/
inline macrostep begin_initial(const machine& owner, const boost::json::value& input = nullptr) {
    event initial{.type = std::string(init_event_type), .payload = {}};
    if (!input.is_null()) {
        initial.payload.emplace("input", input);
    }
    boost::json::value context = owner.context();
    std::optional<boost::system::error_code> refused;
    if (const std::optional<context_maker>& maker = owner.registry().context; maker.has_value()) {
        context = boost::json::object{};
        result<boost::json::value> made = (*maker)(input);
        if (!made.has_value()) {
            refused = made.error();
        } else if (!made->is_object()) {
            refused = make_error_code(errc::implementation_failed);
        } else {
            context = std::move(*made);
        }
    }
    snapshot pre_initial =
        detail::make_snapshot(owner, {0}, std::move(context), status::active, std::nullopt, {});
    macrostep cursor(owner, std::move(pre_initial), std::move(initial), macrostep::phase::initial);
    cursor.refused_ = refused;
    return cursor;
}

/**
 Makes the cursor of the macrostep an event runs from a snapshot.

 Its first @ref macrostep::next runs the event's microstep. An event that
 no transition takes leaves the snapshot unchanged and reports an empty
 list of transitions. The macrostep settles there unless the event enables
 an eventless transition, which a guard that reads the event can do:
 XState's macrostep selects the eventless transitions after the event's
 microstep, whether or not that microstep took a transition.

 @param owner The machine.
 @param from The snapshot the macrostep begins from.
 @param happened The event.
 @return The cursor, which has run no microstep yet.
 @pre `from` is a snapshot of `owner`.

 @see "One microstep at a time", in the guide.
*/
inline macrostep begin(const machine& owner, snapshot from, event happened) {
    return {owner, std::move(from), std::move(happened), macrostep::phase::external};
}

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_MACROSTEP_HPP
