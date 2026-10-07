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

/** The name XState gives a snapshot's status. */
std::string_view status_name(xstate::status status) {
    switch (status) {
        case xstate::status::active: return "active";
        case xstate::status::done: return "done";
        case xstate::status::error: return "error";
    }
    return "unknown";
}

}  // namespace

int main() {
    // tag::example[]
    xstate::implementations implementations;
    implementations.actions.emplace(
        "count",
        xstate::assign_action{
            .assignment = [](const xstate::action_args&) -> xstate::result<boost::json::object> {
                return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
            },
        });
    const boost::json::value config = boost::json::parse(R"({
        "id": "m",
        "initial": "idle",
        "context": {"count": 0},
        "states": {
            "idle": {"on": {"GO": {"target": "busy", "actions": "count"}}},
            "busy": {}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, implementations);
    if (!machine.has_value()) {
        return 1;
    }
    const auto [next, actions] =
        xstate::transition(*machine, xstate::get_initial_snapshot(*machine),
                           xstate::event{.type = "GO", .payload = {}});
    std::cout << "transition() returns: " << status_name(next.status) << ", "
              << boost::json::serialize(next.value) << ", " << next.error.message() << ", "
              << actions.size() << " actions\n";

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*machine);
    if (!actor.has_value()) {
        return 1;
    }
    if (!system.start(*actor).has_value()) {
        return 1;
    }
    if (!system.send(*actor, xstate::event{.type = "GO", .payload = {}}).has_value()) {
        return 1;
    }
    const xstate::snapshot* failed = system.snapshot_of(*actor);
    if (failed == nullptr) {
        return 1;
    }
    std::cout << "an actor's snapshot: " << status_name(failed->status) << ", "
              << boost::json::serialize(failed->value) << ", " << failed->error.message() << '\n';
    // end::example[]
    return 0;
}
