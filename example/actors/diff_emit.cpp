// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    xstate::implementations implementations;
    implementations.actions.emplace(
        "emitStar", xstate::emit_action{
                        .event = [](const xstate::action_args&) -> xstate::result<xstate::event> {
                            return xstate::event{.type = "*", .payload = {}};
                        },
                    });
    const boost::json::value config = boost::json::parse(R"({
        "on": {"PING": {"actions": "emitStar"}}
    })");
    const xstate::result<xstate::machine> emitter = xstate::create_machine(config, implementations);
    if (!emitter.has_value()) {
        return 1;
    }
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*emitter);
    if (!actor.has_value()) {
        return 1;
    }
    const xstate::result<void> listening = system.on_emitted(
        *actor, [](const xstate::event& heard) { std::cout << "heard " << heard.type << '\n'; });
    if (!listening.has_value()) {
        return 1;
    }
    if (!system.start(*actor).has_value()) {
        return 1;
    }
    if (!system.send(*actor, xstate::event{.type = "PING", .payload = {}}).has_value()) {
        return 1;
    }
    // end::example[]
    return 0;
}
