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

 Tip: after each microstep the cursor already selects the eventless
 transitions XState's next iteration would take, so `settled` is known on
 the microstep that closes the macrostep and the next call does not select
 them again.
*/
#ifndef WEBCPP_XSTATE_MACROSTEP_HPP
#define WEBCPP_XSTATE_MACROSTEP_HPP

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
 One microstep: the event it ran for, the transitions it took (none for the
 initial one, which XState does not report), the actions it returned and
 the snapshot it left.

 Tip: a transition belongs to the machine and lives as long as any copy of
 it does.
*/
struct microstep {
    xstate::event event{};
    std::optional<std::vector<const transition_definition*>> transitions{};
    std::vector<action> actions{};
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
 What one next() did: its microstep, and whether it settled the macrostep.
 A microstep that fails reports no actions, as XState's pure functions
 throw; `resolved_before_failure` holds those it resolved before the
 failure, of which a running actor of XState has run the custom actions and
 logs, and the macrostep's result holds none of them.
*/
struct progress {
    microstep step{};
    bool settled = false;
    std::vector<action> resolved_before_failure{};
};

/**
 A settled macrostep: the stable snapshot, every action its microsteps
 returned, in order, and how many microsteps it took.
*/
struct macrostep_result {
    xstate::snapshot snapshot{};
    std::vector<action> actions{};
    std::size_t microsteps = 0;
};

class macrostep {
public:
    /** Whether the macrostep has settled; next() then does nothing. */
    [[nodiscard]] bool done() const noexcept { return settled_; }

    /** The snapshot as the last microstep left it. */
    [[nodiscard]] const snapshot& current() const noexcept { return current_; }

    /** The snapshot the macrostep began from. */
    [[nodiscard]] const snapshot& began_from() const noexcept { return start_; }

    /** The settled macrostep; meaningful once done(). */
    [[nodiscard]] const macrostep_result& result() const noexcept { return result_; }

    /**
     Runs exactly one microstep: the initial one, the external event's, or
     one iteration of XState's loop, an enabled eventless transition_definition first
     and a raised event otherwise.

     Tip: when an implementation fails, the macrostep settles with the
     snapshot it began from, its status error, as XState's actor keeps the
     snapshot from before the macrostep that threw.
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
 The cursor of a machine's initial macrostep: the initial microstep, then
 XState's loop for the event xstate.init, which carries `input`; the context
 is computed from it when the implementations say how (StateMachine.ts
 getInitialSnapshot and _getPreInitialState).

 Tip: a context that cannot be computed fails the macrostep on its first
 next(), with the snapshot before it, as XState's getInitialSnapshot catches
 the error into the snapshot it returns.
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

/** The cursor of the macrostep `happened` runs from `from`. */
inline macrostep begin(const machine& owner, snapshot from, event happened) {
    return {owner, std::move(from), std::move(happened), macrostep::phase::external};
}

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_MACROSTEP_HPP
