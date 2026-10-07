// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string>
#include <string_view>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "dog",
        "initial": "asleep",
        "states": {
            "asleep": {"on": {"wakes up": "awake"}},
            "awake": {"on": {"falls asleep": "asleep"}}
        }
    })");
    const xstate::result<xstate::machine> dog = xstate::create_machine(config, {});
    if (!dog.has_value()) {
        return 1;
    }

    xstate::snapshot now = xstate::get_initial_snapshot(*dog);
    for (const std::string_view type : {"wakes up", "wakes up", "falls asleep"}) {
        const xstate::event happened{.type = std::string(type), .payload = {}};
        now = xstate::get_next_snapshot(*dog, now, happened);
        std::cout << type << " -> " << boost::json::serialize(now.value) << '\n';
    }
    // end::example[]
    return 0;
}
