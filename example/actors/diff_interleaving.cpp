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
    const boost::json::value kid_config = boost::json::parse(R"({
        "entry": "kidStarted"
    })");
    const xstate::result<xstate::machine> kid = xstate::create_machine(kid_config, {});
    if (!kid.has_value()) {
        return 1;
    }
    xstate::implementations implementations;
    implementations.actors.emplace("kid", xstate::machine_actor{*kid});
    const boost::json::value app_config = boost::json::parse(R"({
        "entry": "appStarted",
        "invoke": {"id": "kid", "src": "kid"}
    })");
    const xstate::result<xstate::machine> app = xstate::create_machine(app_config, implementations);
    if (!app.has_value()) {
        return 1;
    }
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<void> acting = system.on_action(
        [](xstate::actor_ref, const xstate::action& custom) { std::cout << custom.type << '\n'; });
    if (!acting.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*app);
    if (!actor.has_value()) {
        return 1;
    }
    if (!system.start(*actor).has_value()) {
        return 1;
    }
    // end::example[]
    return 0;
}
