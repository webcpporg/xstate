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
#include <utility>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

// tag::advance[]
/**
 Gives the clock every action a macrostep returned: it schedules the built-in
 delayed raises and applies the built-in cancels.
*/
void schedule(xstate::simulated_clock& clock, const std::vector<xstate::action>& actions) {
    for (const xstate::action& returned : actions) {
        clock.apply(returned);
    }
}

/**
 Moves the clock forward and delivers each event that comes due, as a
 macrostep of its own, scheduling what that macrostep returns.
*/
void advance(const xstate::machine& machine, xstate::snapshot& current,
             xstate::simulated_clock& clock, std::uint64_t milliseconds) {
    clock.increment(milliseconds);
    while (std::optional<xstate::event> due = clock.pop_due()) {
        auto [next, actions] = xstate::transition(machine, current, *due);
        schedule(clock, actions);
        current = std::move(next);
    }
}

// end::advance[]

}  // namespace

// tag::main[]
int main() {
    const boost::json::value config = boost::json::parse(R"({
        "id": "light",
        "initial": "green",
        "states": {
            "green": {"after": {"1000": "yellow"}},
            "yellow": {"after": {"1000": "red"}},
            "red": {"after": {"1000": "green"}}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }

    xstate::simulated_clock clock;
    auto [current, actions] = xstate::initial_transition(*machine);
    schedule(clock, actions);

    advance(*machine, current, clock, 500);
    std::cout << "after 500 ms: " << boost::json::serialize(current.value) << '\n';
    if (current.value != boost::json::value("green")) {
        return 1;
    }

    advance(*machine, current, clock, 510);
    std::cout << "after 1010 ms: " << boost::json::serialize(current.value) << '\n';
    if (current.value != boost::json::value("yellow")) {
        return 1;
    }
    return 0;
}

// end::main[]
