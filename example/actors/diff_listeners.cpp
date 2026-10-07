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
        "ping", xstate::emit_action{
                    .event = [](const xstate::action_args&) -> xstate::result<xstate::event> {
                        return xstate::event{.type = "PING", .payload = {}};
                    },
                });
    const boost::json::value config = boost::json::parse(R"({
        "on": {"GO": {"actions": "ping"}}
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
    const xstate::result<void> hearing_any =
        system.on_emitted(*actor, [](const xstate::event& heard) {
            std::cout << "the listener of every type heard " << heard.type << '\n';
        });
    if (!hearing_any.has_value()) {
        return 1;
    }
    // XState's actor.on('PING', ...): a listener that tests the type itself.
    const xstate::result<void> hearing_ping =
        system.on_emitted(*actor, [](const xstate::event& heard) {
            if (heard.type != "PING") {
                return;
            }
            std::cout << "the listener of PING heard " << heard.type << '\n';
        });
    if (!hearing_ping.has_value()) {
        return 1;
    }
    if (!system.start(*actor).has_value()) {
        return 1;
    }
    if (!system.send(*actor, xstate::event{.type = "GO", .payload = {}}).has_value()) {
        return 1;
    }
    // end::example[]
    return 0;
}
