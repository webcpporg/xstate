// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    const boost::json::value config = boost::json::parse(R"({
        "id": "toggle",
        "initial": "Inactive",
        "states": {
            "Inactive": {"on": {"toggle": "Active"}},
            "Active": {"on": {"toggle": "Inactive"}}
        }
    })");
    const xstate::result<xstate::machine> toggle_machine = xstate::create_machine(config, {});
    if (!toggle_machine.has_value()) {
        return 1;
    }

    // tag::actor[]
    xstate::actor_system system({.fuel = 1000});

    /** Whether a call that delivers ran to its end: no error, and no actor left parked. */
    const auto settled = [](const xstate::result<xstate::run_outcome>& ran) {
        return ran.has_value() && *ran == xstate::run_outcome::settled;
    };

    const xstate::result<xstate::actor_ref> actor = system.create_actor(*toggle_machine);
    if (!actor.has_value()) {
        return 1;
    }

    const xstate::result<void> subscribed =
        system.subscribe(*actor, [](const xstate::snapshot& snapshot) {
            std::cout << "Value: " << boost::json::serialize(snapshot.value) << '\n';
        });
    if (!subscribed.has_value()) {
        return 1;
    }

    if (!settled(system.start(*actor))) {  // logs "Inactive"
        return 1;
    }

    const xstate::event toggle{.type = "toggle", .payload = {}};
    if (!settled(system.send(*actor, toggle))) {  // logs "Active"
        return 1;
    }
    if (!settled(system.send(*actor, toggle))) {  // logs "Inactive"
        return 1;
    }

    const xstate::snapshot* now = system.snapshot_of(*actor);
    if (now == nullptr) {
        return 1;
    }
    std::cout << "Now: " << boost::json::serialize(now->value) << '\n';
    // end::actor[]
    return 0;
}
