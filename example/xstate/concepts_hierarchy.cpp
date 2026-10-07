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
        "id": "walk",
        "initial": "waiting",
        "states": {
            "waiting": {"on": {"leave home": "onAWalk"}},
            "onAWalk": {
                "initial": "walking",
                "states": {
                    "walking": {"on": {"speed up": "running"}},
                    "running": {"on": {"slow down": "walking"}}
                },
                "on": {"arrive home": "walkComplete"}
            },
            "walkComplete": {}
        }
    })");
    const xstate::result<xstate::machine> walk = xstate::create_machine(config, {});
    if (!walk.has_value()) {
        return 1;
    }

    xstate::snapshot now = xstate::get_initial_snapshot(*walk);
    for (const std::string_view type : {"leave home", "speed up", "arrive home"}) {
        const xstate::event happened{.type = std::string(type), .payload = {}};
        now = xstate::get_next_snapshot(*walk, now, happened);
        std::cout << type << " -> " << boost::json::serialize(now.value) << std::boolalpha
                  << ", on a walk: " << now.matches("onAWalk") << '\n';
    }
    // end::example[]
    return 0;
}
