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
        "id": "light",
        "initial": "green",
        "states": {
            "green": {"on": {"TIMER": "yellow"}},
            "yellow": {"on": {"TIMER": "red"}},
            "red": {"on": {"TIMER": "green"}}
        }
    })");
    const xstate::result<xstate::machine> light = xstate::create_machine(config, {});
    if (!light.has_value()) {
        return 1;
    }
    const xstate::event timer{.type = "TIMER", .payload = {}};
    const xstate::event unknown{.type = "UNKNOWN", .payload = {}};

    const xstate::snapshot green = xstate::get_initial_snapshot(*light);
    const xstate::snapshot yellow = xstate::get_next_snapshot(*light, green, timer);
    const xstate::snapshot red = xstate::get_next_snapshot(*light, yellow, timer);
    const xstate::snapshot still_red = xstate::get_next_snapshot(*light, red, unknown);

    std::cout << boost::json::serialize(green.value) << '\n';
    std::cout << boost::json::serialize(yellow.value) << '\n';
    std::cout << boost::json::serialize(red.value) << '\n';
    std::cout << boost::json::serialize(still_red.value) << '\n';
    // end::example[]
    return 0;
}
