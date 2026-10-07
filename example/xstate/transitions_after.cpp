// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

// tag::print[]
/** Prints a macrostep's actions, one a line: its type, and its params when it has some. */
void print_actions(std::string_view macrostep, const std::vector<xstate::action>& actions) {
    std::cout << macrostep << ":\n";
    for (const xstate::action& one : actions) {
        std::cout << "  " << one.type;
        if (!one.params.is_null()) {
            std::cout << ' ' << boost::json::serialize(one.params);
        }
        std::cout << '\n';
    }
}

// end::print[]
// tag::helpers[]
/** Hands each action to the clock and runs the custom ones, which here print their name. */
void execute(const std::vector<xstate::action>& actions, xstate::simulated_clock& clock) {
    for (const xstate::action& one : actions) {
        clock.apply(one);
        if (!one.builtin) {
            std::cout << one.type << '\n';
        }
    }
}

/** Moves the clock to `time` and delivers each event that comes due, a macrostep each. */
xstate::result<xstate::snapshot> advance(const xstate::machine& machine, xstate::snapshot now,
                                         xstate::simulated_clock& clock, std::uint64_t time) {
    if (const xstate::result<void> moved = clock.set(time); !moved.has_value()) {
        return moved.error();
    }
    while (const std::optional<xstate::event> due = clock.pop_due()) {
        auto [next, actions] = xstate::transition(machine, now, *due);
        execute(actions, clock);
        now = std::move(next);
    }
    return now;
}

// end::helpers[]

}  // namespace

int main() {
    // tag::config[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "game",
        "initial": "waitingForButtonPush",
        "states": {
            "waitingForButtonPush": {
                "after": {
                    "5000": {"target": "timedOut", "actions": "logThatYouGotTimedOut"}
                },
                "on": {
                    "PUSH_BUTTON": {"actions": "logSuccess", "target": "success"}
                }
            },
            "success": {},
            "timedOut": {}
        }
    })");
    const xstate::result<xstate::machine> game = xstate::create_machine(config, {});
    if (!game.has_value()) {
        return 1;
    }
    // end::config[]

    // tag::actions[]
    const auto [waiting, entered] = xstate::initial_transition(*game);
    print_actions("initial macrostep", entered);
    const xstate::event push{.type = "PUSH_BUTTON", .payload = {}};
    print_actions("PUSH_BUTTON", xstate::transition(*game, waiting, push).second);
    // end::actions[]

    // tag::clock[]
    /** A player who never pushes. */
    {
        xstate::simulated_clock clock;
        auto [now, actions] = xstate::initial_transition(*game);
        execute(actions, clock);
        std::cout << "at 0: " << boost::json::serialize(now.value) << '\n';
        for (const std::uint64_t time : {4999, 5000}) {
            const xstate::result<xstate::snapshot> later = advance(*game, now, clock, time);
            if (!later.has_value()) {
                return 1;
            }
            now = *later;
            std::cout << "at " << time << ": " << boost::json::serialize(now.value) << '\n';
        }
    }

    /** A player who pushes the button at 1000. */
    {
        xstate::simulated_clock clock;
        auto [now, actions] = xstate::initial_transition(*game);
        execute(actions, clock);
        std::cout << "at 0: " << boost::json::serialize(now.value) << '\n';
        const xstate::result<xstate::snapshot> pushed = advance(*game, now, clock, 1000);
        if (!pushed.has_value()) {
            return 1;
        }
        std::tie(now, actions) =
            xstate::transition(*game, *pushed, {.type = "PUSH_BUTTON", .payload = {}});
        execute(actions, clock);
        std::cout << "at 1000: " << boost::json::serialize(now.value) << '\n';
        const xstate::result<xstate::snapshot> later = advance(*game, now, clock, 6000);
        if (!later.has_value()) {
            return 1;
        }
        std::cout << "at 6000: " << boost::json::serialize(later->value) << '\n';
    }
    // end::clock[]
    return 0;
}
