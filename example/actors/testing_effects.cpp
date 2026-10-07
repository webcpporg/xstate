// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace xstate = webcpp::xstate;

// tag::main[]
int main() {
    const boost::json::value config = boost::json::parse(R"({
        "initial": "idle",
        "states": {
            "idle": {
                "on": {
                    "start": {
                        "target": "running",
                        "actions": {"type": "logMessage", "params": {"message": "Started!"}}
                    }
                }
            },
            "running": {}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    std::vector<std::string> calls;
    const xstate::result<void> acting =
        system.on_action([&calls](xstate::actor_ref, const xstate::action& returned) {
            calls.push_back(returned.type + ' ' + boost::json::serialize(returned.params));
        });
    if (!acting.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*machine);
    if (!actor.has_value() || system.start(*actor) != xstate::run_outcome::settled) {
        return 1;
    }

    const xstate::event start{.type = "start", .payload = {}};
    if (system.send(*actor, start) != xstate::run_outcome::settled) {
        return 1;
    }

    const xstate::snapshot* now = system.snapshot_of(*actor);
    const std::vector<std::string> expected{R"(logMessage {"message":"Started!"})"};
    if (now == nullptr || now->value != boost::json::value("running") || calls != expected) {
        return 1;
    }
    std::cout << "value: " << boost::json::serialize(now->value) << '\n';
    for (const std::string& call : calls) {
        std::cout << call << '\n';
    }
    return 0;
}

// end::main[]
