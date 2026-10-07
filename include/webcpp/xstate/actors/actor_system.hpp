// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 A system of xstate actors on one xactor scheduler, run by its host: it
 creates and starts actors, delivers the host's events, and runs until no
 message is left; XState's createActor and actor system (createActor.ts,
 system.ts).

 Tip: every host call that delivers opens executions of its own, each with
 the budget's fuel, and runs the system before it returns, as XState's
 send() processes at once; a call that leaves an actor parked says so, and
 resume() gives it more fuel. While an actor is parked the host's inputs
 wait, in order, and resume() applies them once it has settled, as XState
 runs each input to its end before the next (doc: #xstate-invariant-a1).
 A callback runs inside an actor's turn: a call from it that changes the
 system is invalid_argument, and the const ones are free.
*/
#ifndef WEBCPP_XSTATE_ACTORS_ACTOR_SYSTEM_HPP
#define WEBCPP_XSTATE_ACTORS_ACTOR_SYSTEM_HPP

#include <webcpp/xactor.hpp>
#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors/host_logic.hpp>
#include <webcpp/xstate/actors/machine_logic.hpp>
#include <webcpp/xstate/actors/message.hpp>
#include <webcpp/xstate/actors/system_state.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace webcpp::xstate {

// The fuel of one execution of the system, xactor's.
using xactor::budgets;

/** How an actor the host creates starts: XState's createActor options. */
struct actor_options {
    boost::json::value input{};
    // The actor's id; when none is given, its session id x:<n>, n the actors
    // the system created or began to create before it, as XState's
    // createActor names a root.
    std::optional<std::string> id{};
    std::optional<std::string> system_id{};
};

/** Whether a run left every actor settled, or one parked for want of fuel. */
enum class run_outcome { settled, out_of_fuel };

class actor_system {
public:
    explicit actor_system(budgets limits) : scheduler_(limits) {}

    // Every actor holds a reference to the system's state.
    actor_system(const actor_system&) = delete;
    actor_system& operator=(const actor_system&) = delete;
    actor_system(actor_system&&) = delete;
    actor_system& operator=(actor_system&&) = delete;
    ~actor_system() = default;

    /**
     Creates an actor that runs `logic`, not yet started; XState's
     createActor. A systemId another running actor holds is refused.
    */
    result<actor_ref> create_actor(const machine& logic, const actor_options& options = {}) {
        if (running_) {
            return xactor::failure<actor_ref>(xactor::errc::invalid_argument);
        }
        // XState registers a systemId only when it is truthy, never the empty one.
        // One initialisation, not a copy then a reset: GCC 14 at -O3 reads the
        // reset copy as maybe uninitialized (GCC bug 80635).
        std::optional<std::string> system_id =
            options.system_id.has_value() && !options.system_id->empty() ? options.system_id
                                                                         : std::nullopt;
        if (system_id.has_value() && state_.registered(*system_id).has_value()) {
            return failure<actor_ref>(errc::system_id_taken);
        }
        std::string id = options.id.value_or("x:" + std::to_string(state_.booked()));
        result<actor_ref> created =
            scheduler_.spawn(std::make_unique<machine_logic>(state_, logic, options.input));
        if (!created.has_value()) {
            return created;
        }
        state_.add(*created, actor_record{
                                 .id = std::move(id),
                                 .src = {},
                                 .system_id = std::move(system_id),
                                 .current = std::nullopt,
                                 .children = {},
                             });
        return created;
    }

    /** Starts an actor: its initial macrostep runs; XState's actor.start(). */
    result<run_outcome> start(actor_ref actor) { return deliver(actor, start_actor{}); }

    /** Sends an actor an event from the host; XState's actor.send(). */
    result<run_outcome> send(actor_ref actor, event happened) {
        return deliver(actor, std::move(happened));
    }

    /**
     Moves time to `now`: releases the timers due by then one at a time, in
     (deadline, arming) order, and runs after each, all in one execution,
     whose fuel bounds a chain of timers that keep arming.

     Tip: one at a time, as XState's SimulatedClock fires them, so a timer's
     handling can cancel a later one of the same tick.
    */
    result<run_outcome> clock_tick(std::uint64_t now) {
        if (running_) {
            return xactor::failure<run_outcome>(xactor::errc::invalid_argument);
        }
        held_.emplace_back(held_tick{.now = now});
        return apply_held();
    }

    /** The host's pending requests, in the order their actors started. */
    [[nodiscard]] std::vector<host_request> host_requests() const { return state_.requests(); }

    /**
     Answers a pending request with its output: its actor finishes and its
     parent receives the actor's done event. invalid_argument, and nothing
     changes, for a request no longer pending.
    */
    result<run_outcome> resolve(const host_request& request,
                                std::optional<boost::json::value> output) {
        return answer(request, host_answer{.resolved = true, .value = std::move(output)});
    }

    /** Answers a pending request with an error, as resolve() does with an output. */
    result<run_outcome> reject(const host_request& request,
                               std::optional<boost::json::value> error) {
        return answer(request, host_answer{.resolved = false, .value = std::move(error)});
    }

    /**
     Gives every parked actor a new execution of its own and runs, then
     applies, in order, the host's inputs held while an actor was parked.
    */
    result<run_outcome> resume() {
        if (running_) {
            return xactor::failure<run_outcome>(xactor::errc::invalid_argument);
        }
        const std::set<std::uint32_t> parked = state_.parked();
        for (const std::uint32_t actor : parked) {
            if (const result<void> delivered =
                    scheduler_.deliver(actor_ref{.value = actor}, xstate::resume{}, book());
                !delivered.has_value()) {
                return delivered.error();
            }
        }
        if (const result<run_outcome> ran = run_until_idle(); !ran.has_value()) {
            return ran;
        }
        return apply_held();
    }

    /** Runs until no message is left. */
    result<run_outcome> run_until_idle() {
        if (running_) {
            return xactor::failure<run_outcome>(xactor::errc::invalid_argument);
        }
        running_ = true;
        xactor::fifo_driver driver(scheduler_);
        const result<std::size_t> ran = driver.run_until_idle(nullptr);
        running_ = false;
        if (!ran.has_value()) {
            return ran.error();
        }
        return outcome();
    }

    /**
     Stops a root actor and its whole family, each after its descendants,
     and returns them; XState's actor.stop(). invalid_argument, and nothing
     changes, for a child, which XState refuses to stop directly.

     Tip: never held behind a parked actor, since stopping is how the host
     ends an actor that would never settle.
    */
    result<std::vector<actor_ref>> stop(actor_ref actor) {
        if (running_ || parent_of(actor).has_value()) {
            return xactor::failure<std::vector<actor_ref>>(xactor::errc::invalid_argument);
        }
        result<std::vector<actor_ref>> stopped = scheduler_.stop(actor);
        if (stopped.has_value()) {
            for (const actor_ref one : *stopped) {
                state_.release(one);
            }
        }
        return stopped;
    }

    /**
     A machine actor's snapshot once it has been constructed, as XState's
     getSnapshot after createActor; nullptr before, and for a host actor.
    */
    [[nodiscard]] const snapshot* snapshot_of(actor_ref actor) const {
        const actor_record* record = state_.record_of(actor);
        return record == nullptr || !record->current.has_value() ? nullptr : &*record->current;
    }

    [[nodiscard]] result<xactor::status> status_of(actor_ref actor) const {
        return scheduler_.status_of(actor);
    }

    /** The actor that spawned `actor`; none for one the host created. */
    [[nodiscard]] std::optional<actor_ref> parent_of(actor_ref actor) const {
        const actor_record* record = state_.record_of(actor);
        return record == nullptr ? std::nullopt : record->parent;
    }

    /** The id an actor's parent knows it by, or the host's id for it. */
    [[nodiscard]] std::string_view id_of(actor_ref actor) const {
        const actor_record* record = state_.record_of(actor);
        return record == nullptr ? std::string_view() : std::string_view(record->id);
    }

    /** Hands the system's inspector what every actor does; XState's inspect. */
    result<void> inspect(inspector heard) {
        if (running_) {
            return xactor::failure<void>(xactor::errc::invalid_argument);
        }
        state_.inspect(std::move(heard));
        return {};
    }

    /** The running actor registered under a systemId; XState's system.get. */
    [[nodiscard]] std::optional<actor_ref> get(std::string_view system_id) const {
        return state_.registered(system_id);
    }

    /** The child `parent` knows by `id`, while it has it. */
    [[nodiscard]] std::optional<actor_ref> child_of(actor_ref parent, std::string_view id) const {
        return state_.child_of(parent, id);
    }

    /**
     Hands `listener` each snapshot an actor settles in while it is active
     and the one it is done in, in order; XState's actor.subscribe.
     invalid_argument for an address that names no actor.
    */
    result<void> subscribe(actor_ref actor, system_state::snapshot_listener listener) {
        if (running_) {
            return xactor::failure<void>(xactor::errc::invalid_argument);
        }
        if (const result<xactor::status> known = scheduler_.status_of(actor); !known.has_value()) {
            return known.error();
        }
        state_.subscribe(actor, std::move(listener));
        return {};
    }

    /**
     Hands `listener` each event an actor emits, in order; XState's
     actor.on('*'). invalid_argument for an address that names no actor.
    */
    result<void> on_emitted(actor_ref actor, system_state::event_listener listener) {
        if (running_) {
            return xactor::failure<void>(xactor::errc::invalid_argument);
        }
        if (const result<xactor::status> known = scheduler_.status_of(actor); !known.has_value()) {
            return known.error();
        }
        state_.listen(actor, std::move(listener));
        return {};
    }

    /** Hands every custom action to `listener`, with the actor that returned it. */
    result<void> on_action(system_state::action_listener listener) {
        if (running_) {
            return xactor::failure<void>(xactor::errc::invalid_argument);
        }
        state_.on_action(std::move(listener));
        return {};
    }

    /** Hands every log to `listener`, with the actor that returned it. */
    result<void> on_log(system_state::log_listener listener) {
        if (running_) {
            return xactor::failure<void>(xactor::errc::invalid_argument);
        }
        state_.on_log(std::move(listener));
        return {};
    }

private:
    /** A message the host delivers to an actor. */
    struct held_delivery {
        actor_ref to{};
        actor_message message{};
    };

    /** Time the host brings. */
    struct held_tick {
        std::uint64_t now = 0;
    };

    using held_input = std::variant<held_delivery, held_tick>;

    [[nodiscard]] run_outcome outcome() const {
        return state_.parked().empty() ? run_outcome::settled : run_outcome::out_of_fuel;
    }

    xactor::correlation_id book() noexcept {
        ++executions_;
        return xactor::correlation_id{.value = executions_};
    }

    [[nodiscard]] bool pending(actor_ref actor) const {
        return std::ranges::any_of(state_.requests(),
                                   [actor](const host_request& one) { return one.actor == actor; });
    }

    result<run_outcome> answer(const host_request& request, host_answer given) {
        if (!pending(request.actor)) {
            return xactor::failure<run_outcome>(xactor::errc::invalid_argument);
        }
        return deliver(request.actor, std::move(given));
    }

    /** Queues a message from the host for an actor, then applies what may run. */
    result<run_outcome> deliver(actor_ref actor, actor_message message) {
        if (running_) {
            return xactor::failure<run_outcome>(xactor::errc::invalid_argument);
        }
        if (const result<xactor::status> known = scheduler_.status_of(actor); !known.has_value()) {
            return known.error();
        }
        held_.emplace_back(held_delivery{.to = actor, .message = std::move(message)});
        return apply_held();
    }

    /**
     Applies the host's inputs in order while no actor is parked, each in
     an execution of its own and run before the next.
    */
    result<run_outcome> apply_held() {
        while (state_.parked().empty() && !held_.empty()) {
            held_input next = std::move(held_.front());
            held_.pop_front();
            const result<void> applied =
                std::visit([this](auto& input) { return apply(input); }, next);
            if (!applied.has_value()) {
                return applied.error();
            }
        }
        return outcome();
    }

    /**
     Delivers a held message and runs.

     Tip: an answer to a request its actor no longer waits on is dropped,
     as XState ignores a promise that settles after its actor stopped.
    */
    result<void> apply(held_delivery& input) {
        if (std::holds_alternative<host_answer>(input.message) && !pending(input.to)) {
            return {};
        }
        if (const result<void> delivered =
                scheduler_.deliver(input.to, std::move(input.message), book());
            !delivered.has_value()) {
            return delivered;
        }
        return ran();
    }

    /**
     Releases the timers due by a held tick one at a time, running after
     each; when an actor parks, the rest of the tick waits first in line.
    */
    result<void> apply(const held_tick& input) {
        const xactor::correlation_id tick = book();
        while (true) {
            const result<bool> released = scheduler_.release_next(input.now, tick);
            if (!released.has_value()) {
                return released.error();
            }
            if (!*released) {
                return {};
            }
            if (const result<void> run = ran(); !run.has_value()) {
                return run;
            }
            if (!state_.parked().empty()) {
                held_.emplace_front(input);
                return {};
            }
        }
    }

    result<void> ran() {
        if (const result<run_outcome> run = run_until_idle(); !run.has_value()) {
            return run.error();
        }
        return {};
    }

    // Declared first, so it outlives the actors that refer to it.
    system_state state_;
    xactor::scheduler<actor_message> scheduler_;
    std::uint64_t executions_ = 0;
    // The host's inputs not yet applied, which wait while an actor is parked.
    std::deque<held_input> held_;
    // Whether the system is running an actor's turn, from which a callback
    // may not call it back.
    bool running_ = false;
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_ACTORS_ACTOR_SYSTEM_HPP
