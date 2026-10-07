// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string_view>

namespace xstate = webcpp::xstate;
namespace xactor = webcpp::xactor;

namespace {

std::string_view name_of(xstate::run_outcome outcome) {
    return outcome == xstate::run_outcome::settled ? "settled" : "out_of_fuel";
}

std::string_view name_of(xactor::status status) {
    switch (status) {
        case xactor::status::active: return "active";
        case xactor::status::done: return "done";
        case xactor::status::error: return "error";
        case xactor::status::stopped: return "stopped";
    }
    return "unknown";
}

/** Prints what a host call returned and the state value the actor has published. */
void print(std::string_view call, xstate::run_outcome outcome, const xstate::actor_system& system,
           xstate::actor_ref actor) {
    const xstate::snapshot* current = system.snapshot_of(actor);
    std::cout << call << ": " << name_of(outcome) << ' '
              << (current == nullptr ? "none" : boost::json::serialize(current->value)) << '\n';
}

}  // namespace

int main() {
    // tag::chain[]
    // GO takes five microsteps: idle to a, then four eventless ones.
    const boost::json::value chain_config = boost::json::parse(R"({
        "initial": "idle",
        "states": {
            "idle": {"on": {"GO": "a"}},
            "a": {"always": "b"},
            "b": {"always": "c"},
            "c": {"always": "d"},
            "d": {"always": "e"},
            "e": {"on": {"PING": "f"}},
            "f": {}
        }
    })");
    const xstate::result<xstate::machine> chain = xstate::create_machine(chain_config, {});
    if (!chain.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 3});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*chain);
    if (!actor.has_value()) {
        return 1;
    }
    const xstate::result<xstate::run_outcome> started = system.start(*actor);
    if (!started.has_value()) {
        return 1;
    }
    print("start", *started, system, *actor);  // start: settled "idle"

    const xstate::result<xstate::run_outcome> go =
        system.send(*actor, {.type = "GO", .payload = {}});
    if (!go.has_value()) {
        return 1;
    }
    print("GO", *go, system, *actor);  // GO: out_of_fuel "idle"

    const xstate::result<xstate::run_outcome> ping =
        system.send(*actor, {.type = "PING", .payload = {}});
    if (!ping.has_value()) {
        return 1;
    }
    print("PING", *ping, system, *actor);  // PING: out_of_fuel "idle"

    const xstate::result<xstate::run_outcome> resumed = system.resume();
    if (!resumed.has_value()) {
        return 1;
    }
    print("resume", *resumed, system, *actor);  // resume: settled "f"
    // end::chain[]

    // tag::spin[]
    // An eventless cycle: its initial macrostep never settles.
    const boost::json::value spin_config = boost::json::parse(R"({
        "initial": "ping",
        "states": {
            "ping": {"always": "pong"},
            "pong": {"always": "ping"}
        }
    })");
    const xstate::result<xstate::machine> spin = xstate::create_machine(spin_config, {});
    if (!spin.has_value()) {
        return 1;
    }

    xstate::actor_system bounded({.fuel = 100});
    const xstate::result<xstate::actor_ref> spinner = bounded.create_actor(*spin);
    if (!spinner.has_value()) {
        return 1;
    }
    const xstate::result<xstate::run_outcome> spun = bounded.start(*spinner);
    if (!spun.has_value()) {
        return 1;
    }
    print("start", *spun, bounded, *spinner);  // start: out_of_fuel none
    const xstate::result<xstate::run_outcome> again = bounded.resume();
    if (!again.has_value()) {
        return 1;
    }
    print("resume", *again, bounded, *spinner);  // resume: out_of_fuel none

    if (!bounded.stop(*spinner).has_value()) {
        return 1;
    }
    const xstate::result<xactor::status> status = bounded.status_of(*spinner);
    if (!status.has_value()) {
        return 1;
    }
    std::cout << "stop: " << name_of(*status) << '\n';  // stop: stopped
    // end::spin[]
    return 0;
}
