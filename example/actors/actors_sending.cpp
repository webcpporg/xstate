// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>

namespace xstate = webcpp::xstate;

namespace {

xstate::result<xstate::event> token(const xstate::action_args& /*args*/) {
    return xstate::event{.type = "TOKEN", .payload = {}};
}

xstate::result<xstate::event> code(const xstate::action_args& /*args*/) {
    return xstate::event{.type = "CODE", .payload = {}};
}

}  // namespace

int main() {
    // tag::sending[]
    xstate::implementations server_implementations;
    // The server answers the actor that sent CODE, its parent.
    const xstate::send_parent_action answer{
        .event = token,
        .id = std::nullopt,
        .delay = std::nullopt,
    };
    server_implementations.actions.emplace("answer", answer);
    const boost::json::value server_config = boost::json::parse(R"({
        "id": "server",
        "initial": "waitingForCode",
        "states": {
            "waitingForCode": {"on": {"CODE": {"actions": "answer"}}}
        }
    })");
    const xstate::result<xstate::machine> auth_server_machine =
        xstate::create_machine(server_config, server_implementations);
    if (!auth_server_machine.has_value()) {
        return 1;
    }

    xstate::implementations client_implementations;
    client_implementations.actors.emplace("authServer",
                                          xstate::machine_actor{*auth_server_machine});
    const xstate::send_to_action send_code{
        .target = "auth-server",
        .event = code,
        .id = std::nullopt,
        .delay = std::nullopt,
    };
    client_implementations.actions.emplace("sendCode", send_code);
    const boost::json::value client_config = boost::json::parse(R"({
        "id": "client",
        "initial": "idle",
        "states": {
            "idle": {"on": {"AUTH": {"target": "authorizing"}}},
            "authorizing": {
                "invoke": {"id": "auth-server", "src": "authServer"},
                "entry": "sendCode",
                "on": {"TOKEN": {"target": "authorized"}}
            },
            "authorized": {"type": "final"}
        }
    })");
    const xstate::result<xstate::machine> auth_client_machine =
        xstate::create_machine(client_config, client_implementations);
    if (!auth_client_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> client = system.create_actor(*auth_client_machine);
    if (!client.has_value()) {
        return 1;
    }
    const xstate::result<void> subscribed =
        system.subscribe(*client, [](const xstate::snapshot& snapshot) {
            std::cout << boost::json::serialize(snapshot.value) << '\n';
        });
    if (!subscribed.has_value()) {
        return 1;
    }
    if (!system.start(*client).has_value()) {
        return 1;
    }
    if (!system.send(*client, {.type = "AUTH", .payload = {}}).has_value()) {
        return 1;
    }
    // logs "idle", "authorizing", then "authorized"
    // end::sending[]
    return 0;
}
