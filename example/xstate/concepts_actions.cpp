// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "dog",
        "initial": "asleep",
        "states": {
            "asleep": {
                "exit": "stretch",
                "on": {"wakes up": {"target": "awake", "actions": "yawn"}}
            },
            "awake": {"entry": {"type": "bark", "params": {"times": 2}}}
        }
    })");
    const xstate::result<xstate::machine> dog = xstate::create_machine(config, {});
    if (!dog.has_value()) {
        return 1;
    }

    const xstate::snapshot asleep = xstate::get_initial_snapshot(*dog);
    const xstate::event wakes_up{.type = "wakes up", .payload = {}};
    const auto [awake, actions] = xstate::transition(*dog, asleep, wakes_up);
    std::cout << "value: " << boost::json::serialize(awake.value) << '\n';
    for (const xstate::action& returned : actions) {
        std::cout << "the caller executes " << returned.type;
        if (!returned.params.is_null()) {
            std::cout << ' ' << boost::json::serialize(returned.params);
        }
        std::cout << '\n';
    }
    // end::example[]
    return 0;
}
