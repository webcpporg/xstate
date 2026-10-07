// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "loop",
        "initial": "idle",
        "states": {
            "idle": {"on": {"start": "ping"}},
            "ping": {"always": "pong"},
            "pong": {"always": "ping"}
        }
    })");
    const xstate::result<xstate::machine> loop = xstate::create_machine(config, {});
    if (!loop.has_value()) {
        return 1;
    }

    const xstate::snapshot idle = xstate::get_initial_snapshot(*loop);
    xstate::macrostep step = xstate::begin(*loop, idle, {.type = "start", .payload = {}});
    std::size_t spent = 0;
    for (std::size_t fuel = 5; fuel > 0 && !step.done(); --fuel) {
        const xstate::progress made = step.next();
        ++spent;
        std::cout << boost::json::serialize(made.step.snapshot.value) << '\n';
    }
    std::cout << (step.done() ? "settled" : "not settled") << " after " << spent << " microsteps\n";
    // end::example[]
    return 0;
}
