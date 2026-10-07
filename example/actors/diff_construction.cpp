// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string>
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

// tag::describe[]
/** Prints the actor's state value, if it has a snapshot, and whether "worker" is registered. */
void describe(std::string_view moment, const xstate::actor_system& system, xstate::actor_ref app) {
    const xstate::snapshot* now = system.snapshot_of(app);
    const std::string value = now == nullptr ? "no snapshot" : boost::json::serialize(now->value);
    const std::string_view worker = system.get("worker").has_value() ? "registered" : "unknown";
    std::cout << moment << ": " << value << ", worker " << worker << '\n';
}

// end::describe[]

}  // namespace

int main() {
    // tag::example[]
    const boost::json::value worker_config = boost::json::parse("{}");
    const xstate::result<xstate::machine> worker = xstate::create_machine(worker_config, {});
    if (!worker.has_value()) {
        return 1;
    }
    xstate::implementations implementations;
    implementations.actors.emplace("worker", xstate::machine_actor{*worker});
    const boost::json::value config = boost::json::parse(R"({
        "initial": "running",
        "states": {
            "running": {"invoke": {"id": "w", "src": "worker", "systemId": "worker"}}
        }
    })");
    const xstate::result<xstate::machine> app = xstate::create_machine(config, implementations);
    if (!app.has_value()) {
        return 1;
    }
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*app);
    if (!actor.has_value()) {
        return 1;
    }
    describe("before start", system, *actor);
    if (!system.start(*actor).has_value()) {
        return 1;
    }
    describe("after start", system, *actor);
    // end::example[]
    return 0;
}
