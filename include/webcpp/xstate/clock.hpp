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

 Tip: the machine never reads a clock. A caller schedules what a step
 returned, moves the time, and delivers each due event as a macrostep of
 its own, which may schedule more.
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

class simulated_clock {
public:
    [[nodiscard]] std::uint64_t now() const noexcept { return now_; }

    /**
     Holds `delayed` until `delay` milliseconds from now; a later schedule
     with the same id takes the id over, and the earlier one can no longer
     be cancelled, as in XState's scheduler.
    */
    void schedule(event delayed, std::uint64_t delay, std::optional<std::string> id) {
        hold(std::move(delayed), delay, std::move(id));
    }

    /**
     Applies what a step returned to the clock: an xstate.raise with a delay
     is scheduled and an xstate.cancel cancels. A delayed sendTo, a
     sendParent included, is not the machine's to receive, but its timer
     holds its id, as XState's scheduler keys every delayed event by its id.
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

    /** Forgets the event scheduled under `id`; XState's scheduler.cancel. */
    void cancel(std::string_view id) {
        std::erase_if(timers_, [id](const timer& held) { return held.id == id; });
    }

    /** Moves the time to `time`; never backwards, as XState's set refuses. */
    result<void> set(std::uint64_t time) {
        if (time < now_) {
            return failure<void>(errc::invalid_config);
        }
        now_ = time;
        return {};
    }

    /** Moves the time forward by `milliseconds`. */
    void increment(std::uint64_t milliseconds) noexcept { now_ += milliseconds; }

    /**
     The earliest event whose time has come, by deadline then by the order
     it was scheduled in, taken out of the clock; nothing when none is due.
     The id it was scheduled under then cancels nothing, as XState's firing
     timeout deletes its entry of timerMap.
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
