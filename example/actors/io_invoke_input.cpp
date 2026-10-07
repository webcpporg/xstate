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
    // tag::invoke[]
    xstate::implementations implementations;
    // The fetch is the host's: a host actor, XState's fromPromise.
    implementations.actors.emplace("liveFeedback", xstate::host_actor{});
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "invoke": {
            "src": "liveFeedback",
            "input": {"domain": "stately.ai"}
        }
    })");
    const xstate::result<xstate::machine> feedback_machine =
        xstate::create_machine(config, implementations);
    if (!feedback_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> feedback_actor = system.create_actor(*feedback_machine);
    if (!feedback_actor.has_value() || !system.start(*feedback_actor).has_value()) {
        return 1;
    }
    for (const xstate::host_request& request : system.host_requests()) {
        if (request.input.has_value()) {
            std::cout << boost::json::serialize(*request.input) << '\n';
        }
    }
    // end::invoke[]
    return 0;
}
