// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

namespace {

xstate::result<xstate::event> activated(const xstate::action_args& /*args*/) {
    return xstate::event{.type = "activated", .payload = {}};
}

}  // namespace

int main() {
    xstate::implementations implementations;
    implementations.actions.emplace("announce", xstate::emit_action{.event = activated});
    const boost::json::value config = boost::json::parse(R"({
        "initial": "inactive",
        "states": {
            "inactive": {"on": {"toggle": "active"}},
            "active": {"entry": "announce", "on": {"toggle": "inactive"}}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, implementations);
    if (!machine.has_value()) {
        return 1;
    }

    // tag::inspect[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<void> inspecting = system.inspect(xstate::inspector{
        .started = [](xstate::actor_ref) { std::cout << "started\n"; },
        .settled =
            [](xstate::actor_ref, const xstate::machine&, const xstate::event& cause,
               const xstate::snapshot& settled) {
                std::cout << "settled " << cause.type << ' '
                          << boost::json::serialize(settled.value) << '\n';
            },
        .emitted =
            [](xstate::actor_ref, const xstate::event& emitted) {
                std::cout << "emitted " << emitted.type << '\n';
            },
    });
    if (!inspecting.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*machine);
    if (!actor.has_value()) {
        return 1;
    }
    if (!system.start(*actor).has_value()) {
        return 1;
    }
    if (!system.send(*actor, {.type = "toggle", .payload = {}}).has_value()) {
        return 1;
    }
    // end::inspect[]
    return 0;
}
