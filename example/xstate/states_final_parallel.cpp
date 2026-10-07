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
    // tag::final_parallel[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "coffee",
        "initial": "preparing",
        "states": {
            "preparing": {
                "type": "parallel",
                "states": {
                    "grindBeans": {
                        "initial": "grindingBeans",
                        "states": {
                            "grindingBeans": {"on": {"BEANS_GROUND": "beansGround"}},
                            "beansGround": {"type": "final"}
                        }
                    },
                    "boilWater": {
                        "initial": "boilingWater",
                        "states": {
                            "boilingWater": {"on": {"WATER_BOILED": "waterBoiled"}},
                            "waterBoiled": {"type": "final"}
                        }
                    }
                },
                "onDone": "makingCoffee"
            },
            "makingCoffee": {}
        }
    })");
    const xstate::result<xstate::machine> coffee = xstate::create_machine(config, {});
    if (!coffee.has_value()) {
        return 1;
    }
    xstate::snapshot now = xstate::get_initial_snapshot(*coffee);
    std::cout << boost::json::serialize(now.value) << '\n';

    const xstate::event beans_ground{.type = "BEANS_GROUND", .payload = {}};
    now = xstate::get_next_snapshot(*coffee, now, beans_ground);
    std::cout << boost::json::serialize(now.value) << '\n';

    const xstate::event water_boiled{.type = "WATER_BOILED", .payload = {}};
    now = xstate::get_next_snapshot(*coffee, now, water_boiled);
    std::cout << boost::json::serialize(now.value) << '\n';
    // end::final_parallel[]
    return 0;
}
