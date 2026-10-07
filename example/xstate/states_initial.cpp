// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

// tag::types_of[]
/** The types of a list of actions, as a JSON array. */
boost::json::array types_of(const std::vector<xstate::action>& actions) {
    boost::json::array types;
    for (const xstate::action& one : actions) {
        types.emplace_back(one.type);
    }
    return types;
}

// end::types_of[]

}  // namespace

int main() {
    // tag::initial[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "light",
        "initial": "green",
        "states": {
            "green": {"on": {"timer": "yellow"}},
            "yellow": {"on": {"timer": "red", "emergency": "red.stop"}},
            "red": {
                "initial": {"target": "walk", "actions": "beep"},
                "states": {"walk": {}, "wait": {}, "stop": {}}
            }
        }
    })");
    const xstate::result<xstate::machine> light = xstate::create_machine(config, {});
    if (!light.has_value()) {
        return 1;
    }
    const xstate::snapshot green = xstate::get_initial_snapshot(*light);
    std::cout << boost::json::serialize(green.value) << '\n';

    const xstate::event timer{.type = "timer", .payload = {}};
    const xstate::snapshot yellow = xstate::get_next_snapshot(*light, green, timer);
    std::cout << boost::json::serialize(yellow.value) << '\n';

    const auto [walk, walk_actions] = xstate::transition(*light, yellow, timer);
    std::cout << boost::json::serialize(walk.value) << ' ' << types_of(walk_actions) << '\n';

    const xstate::event emergency{.type = "emergency", .payload = {}};
    const auto [stop, stop_actions] = xstate::transition(*light, yellow, emergency);
    std::cout << boost::json::serialize(stop.value) << ' ' << types_of(stop_actions) << '\n';
    // end::initial[]
    return 0;
}
