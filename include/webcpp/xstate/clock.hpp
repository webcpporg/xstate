// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 XState's SimulatedClock with its scheduler, as a value the caller owns: it
 holds the delayed events a machine returned, and releases each when the
 time the caller sets reaches it (SimulatedClock.ts, system.ts scheduler;
 doc: #xstate-invariant-13).

 @note The machine never reads a clock. A caller schedules what a step
 returned, moves the time, and delivers each due event as a macrostep of
 its own, which may schedule more.

 @see "Delayed events and the simulated clock", in the guide.
*/
#ifndef WEBCPP_XSTATE_CLOCK_HPP
#define WEBCPP_XSTATE_CLOCK_HPP

#include <webcpp/xstate/actions.hpp>
#include <webcpp/xstate/errors.hpp>
#include <webcpp/xstate/event.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace webcpp::xstate {

/**
 A clock the caller owns, which holds the delayed events of the machine core
 until the time it is set to reaches them.

 It ports XState's `SimulatedClock` with its scheduler. The machine core
 never reads a clock: a delayed event comes back as an action, and a clock
 holds it until the time the caller sets reaches it. Time is in whole
 milliseconds, where XState's takes fractions, and starts at 0. The actor
 layer keeps its timers in xactor instead.

 @see "Delayed events and the simulated clock", in the guide.
 @see "In the machine core", in the guide.
 @see "Time in whole milliseconds", in the guide.
*/
class simulated_clock {
public:
    /**
     The time.

     @return The time, in milliseconds since the clock was made.
    */
    [[nodiscard]] std::uint64_t now() const noexcept { return now_; }

    /**
     Holds an event until a delay from now.

     A later schedule under the same id takes the id over: the earlier event
     still comes due, but a cancel of the id no longer finds it, as in
     XState's scheduler.

     @param delayed The event.
     @param delay The delay, in milliseconds from @ref now.
     @param id The id a @ref cancel names the event by, or none.
    */
    void schedule(event delayed, std::uint64_t delay, std::optional<std::string> id) {
        hold(std::move(delayed), delay, std::move(id));
    }

    /**
     Applies to the clock a built-in action a step returned.

     A built-in `xstate.raise` with a `delay` is scheduled, under its id when
     it has one, and a built-in `xstate.cancel` cancels its `sendId`. A
     delayed built-in `xstate.sendTo` with an id, a sendParent's included, is
     not the machine's to receive, but holds its id until it comes due, as
     XState's scheduler keys every delayed event by its id. A raise without
     a delay, which its macrostep already ran, and every other action are
     ignored.

     @note A custom action is ignored whatever its type, a config being free
     to name one `xstate.raise`: XState's scheduler, too, holds only what its
     built-in actions schedule.

     @param returned An action a step returned.
    */
    void apply(const action& returned) {
        // A custom action is the caller's, whatever the config named it.
        if (!returned.builtin || !returned.params.is_object()) {
            return;
        }
        const boost::json::object& params = returned.params.get_object();
        if (returned.type == "xstate.cancel") {
            if (const boost::json::value* id = params.if_contains("sendId");
                id != nullptr && id->is_string()) {
                cancel(id->get_string());
            }
            return;
        }
        const boost::json::value* delay = params.if_contains("delay");
        const boost::json::value* raised = params.if_contains("event");
        const boost::json::value* named = params.if_contains("id");
        const result<std::uint64_t> milliseconds =
            delay == nullptr ? failure<std::uint64_t>(errc::invalid_config)
                             : boost::json::try_value_to<std::uint64_t>(*delay);
        if (!milliseconds.has_value() || raised == nullptr) {
            return;
        }
        std::optional<std::string> id;
        if (named != nullptr && named->is_string()) {
            id = std::string(named->get_string());
        }
        if (returned.type == "xstate.sendTo" && id.has_value()) {
            hold(std::nullopt, *milliseconds, std::move(id));
            return;
        }
        if (returned.type != "xstate.raise") {
            return;
        }
        if (const result<event> made = event_from_json(*raised); made.has_value()) {
            hold(*made, *milliseconds, std::move(id));
        }
    }

    /**
     Forgets the event scheduled under an id, if the id still names one.

     It ports XState's `scheduler.cancel`.

     @param id The id.
    */
    void cancel(std::string_view id) {
        std::erase_if(timers_, [id](const timer& held) { return held.id == id; });
    }

    /**
     Sets the time, which never goes back.

     It ports XState's `SimulatedClock.set`, which refuses to go back in
     time.

     @param time The time, in milliseconds.
     @return Success; @ref errc::invalid_config when `time` is earlier than
     @ref now, which changes nothing.
    */
    result<void> set(std::uint64_t time) {
        if (time < now_) {
            return failure<void>(errc::invalid_config);
        }
        now_ = time;
        return {};
    }

    /**
     Moves the time forward.

     It ports XState's `SimulatedClock.increment`.

     @param milliseconds How far.
    */
    void increment(std::uint64_t milliseconds) noexcept { now_ += milliseconds; }

    /**
     Takes the earliest event whose time has come out of the clock.

     The earliest is by deadline, then by the order the events were
     scheduled in, a rule where XState leaves equal deadlines to its sort.
     The id it was scheduled under then cancels nothing, as XState's firing
     timeout deletes its entry of `timerMap`. The caller delivers each due
     event as a macrostep of its own and applies what that returns before
     the next call, so an event it schedules or cancels is ordered as XState
     orders it.

     @return The event; none when no event is due.

     @see "Equal deadlines", in the guide.
    */
    std::optional<event> pop_due() {
        while (true) {
            const auto due =
                std::ranges::min_element(timers_, [](const timer& left, const timer& right) {
                    return left.deadline != right.deadline ? left.deadline < right.deadline
                                                           : left.sequence < right.sequence;
                });
            if (due == timers_.end() || due->deadline > now_) {
                return std::nullopt;
            }
            std::optional<event> released = std::move(due->delayed);
            const std::optional<std::string> armed_as = std::move(due->armed_as);
            timers_.erase(due);
            if (armed_as.has_value()) {
                // XState deletes timerMap's entry for the id, whichever timer holds it.
                for (timer& held : timers_) {
                    if (held.id == armed_as) {
                        held.id.reset();
                    }
                }
            }
            if (released.has_value()) {
                return released;
            }
        }
    }

private:
    /**
     Holds a timer under `id`, which a later one under the same id takes
     over; none for a send to another actor, which delivers nothing here.
    */
    void hold(std::optional<event> delayed, std::uint64_t delay, std::optional<std::string> id) {
        if (id.has_value()) {
            for (timer& held : timers_) {
                if (held.id == id) {
                    held.id.reset();
                }
            }
        }
        timers_.push_back(timer{
            .deadline = now_ + delay,
            .sequence = next_sequence_++,
            .delayed = std::move(delayed),
            .id = id,
            .armed_as = std::move(id),
        });
    }

    struct timer {
        std::uint64_t deadline{};
        std::uint64_t sequence{};
        // None for a send to another actor, whose timer only holds its id.
        std::optional<event> delayed{};
        // The id a cancel finds it by, while it is the id's latest.
        std::optional<std::string> id{};
        // The id it was scheduled under, which its firing unregisters.
        std::optional<std::string> armed_as{};
    };

    std::uint64_t now_ = 0;
    std::uint64_t next_sequence_ = 0;
    std::vector<timer> timers_;
};

}  // namespace webcpp::xstate

#endif  // WEBCPP_XSTATE_CLOCK_HPP
