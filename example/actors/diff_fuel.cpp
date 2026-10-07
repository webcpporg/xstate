// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

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
        "initial": "ping",
        "states": {
            "ping": {"always": "pong"},
            "pong": {"always": "ping"}
        }
    })");
    const xstate::result<xstate::machine> ping_pong = xstate::create_machine(config, {});
    if (!ping_pong.has_value()) {
        return 1;
    }
    xstate::actor_system system({.fuel = 100});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*ping_pong);
    if (!actor.has_value()) {
        return 1;
    }
    const xstate::result<xstate::run_outcome> started = system.start(*actor);
    if (!started.has_value()) {
        return 1;
    }
    std::cout << "start(): " << outcome_name(*started) << '\n';
    const xstate::result<xstate::run_outcome> resumed = system.resume();
    if (!resumed.has_value()) {
        return 1;
    }
    std::cout << "resume(): " << outcome_name(*resumed) << '\n';
    if (!system.stop(*actor).has_value()) {
        return 1;
    }
    const xstate::result<webcpp::xactor::status> ended = system.status_of(*actor);
    if (!ended.has_value()) {
        return 1;
    }
    std::cout << "stop(): " << (*ended == webcpp::xactor::status::stopped ? "stopped" : "running")
              << '\n';
    // end::example[]
    return 0;
}
