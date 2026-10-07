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
        "id": "onAWalk",
        "type": "parallel",
        "states": {
            "activity": {
                "initial": "walking",
                "states": {
                    "walking": {"on": {"speed up": "running", "sees a squirrel": "running"}},
                    "running": {"on": {"slow down": "walking"}}
                }
            },
            "tail": {
                "initial": "notWagging",
                "states": {
                    "notWagging": {"on": {"sees a squirrel": "wagging"}},
                    "wagging": {"on": {"calms down": "notWagging"}}
                }
            }
        }
    })");
    const xstate::result<xstate::machine> walk = xstate::create_machine(config, {});
    if (!walk.has_value()) {
        return 1;
    }

    xstate::snapshot now = xstate::get_initial_snapshot(*walk);
    std::cout << "initial: " << boost::json::serialize(now.value) << '\n';
    for (const std::string_view type : {"sees a squirrel", "slow down"}) {
        const xstate::event happened{.type = std::string(type), .payload = {}};
        now = xstate::get_next_snapshot(*walk, now, happened);
        std::cout << type << " -> " << boost::json::serialize(now.value) << '\n';
    }
    // end::example[]
    return 0;
}
