// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

/** The name of a run's outcome. */
std::string_view outcome_name(xstate::run_outcome outcome) {
    return outcome == xstate::run_outcome::settled ? "settled" : "out_of_fuel";
}

}  // namespace

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "initial": "green",
        "states": {
            "green": {"after": {"100": "yellow"}},
            "yellow": {}
        }
    })");
    const xstate::result<xstate::machine> light = xstate::create_machine(config, {});
    if (!light.has_value()) {
        return 1;
    }
    xstate::actor_system system({.fuel = 10'000});
    if (!system.clock_tick(1000).has_value()) {
        return 1;
    }
    const xstate::result<xstate::run_outcome> back = system.clock_tick(10);
    if (!back.has_value()) {
        return 1;
    }
    std::cout << "clock_tick(10): " << outcome_name(*back) << '\n';
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*light);
    if (!actor.has_value()) {
        return 1;
    }
    // The timer of green is armed now, 100 ms from 10.
    if (!system.start(*actor).has_value()) {
        return 1;
    }
    for (const std::uint64_t time : {1050, 1100}) {
        if (!system.clock_tick(time).has_value()) {
            return 1;
        }
        const xstate::snapshot* now = system.snapshot_of(*actor);
        if (now == nullptr) {
            return 1;
        }
        std::cout << "at " << time << ": " << boost::json::serialize(now->value) << '\n';
    }
    // end::example[]
    return 0;
}
