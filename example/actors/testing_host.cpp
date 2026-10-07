// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <vector>

namespace xstate = webcpp::xstate;

// tag::main[]
int main() {
    xstate::implementations implementations;
    implementations.actors.emplace("fetchData", xstate::host_actor{});
    const boost::json::value config = boost::json::parse(R"({
        "initial": "idle",
        "states": {
            "idle": {"on": {"fetch": "loading"}},
            "loading": {
                "invoke": {"src": "fetchData", "onDone": "success", "onError": "error"}
            },
            "success": {},
            "error": {}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, implementations);
    if (!machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*machine);
    if (!actor.has_value() || system.start(*actor) != xstate::run_outcome::settled) {
        return 1;
    }

    const xstate::event fetch{.type = "fetch", .payload = {}};
    if (system.send(*actor, fetch) != xstate::run_outcome::settled) {
        return 1;
    }

    // The machine waits for the host, which the test plays.
    const std::vector<xstate::host_request> requests = system.host_requests();
    if (requests.size() != 1 || requests.front().src != "fetchData") {
        return 1;
    }
    std::cout << "requests: " << requests.front().src << '\n';
    const boost::json::value output = boost::json::parse(R"({"data": "test"})");
    if (system.resolve(requests.front(), output) != xstate::run_outcome::settled) {
        return 1;
    }

    const xstate::snapshot* now = system.snapshot_of(*actor);
    if (now == nullptr || now->value != boost::json::value("success")) {
        return 1;
    }
    std::cout << "value: " << boost::json::serialize(now->value) << '\n';
    return 0;
}

// end::main[]
