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
        "initial": "onAWalk",
        "states": {
            "onAWalk": {
                "initial": "walking",
                "states": {
                    "walking": {"on": {"speed up": "running"}},
                    "running": {"on": {"slow down": "walking"}},
                    "previous": {"type": "history"}
                },
                "on": {"smells something": "sniffing"}
            },
            "sniffing": {"on": {"carry on": "onAWalk.previous", "start over": "onAWalk"}}
        }
    })");
    const xstate::result<xstate::machine> walk = xstate::create_machine(config, {});
    if (!walk.has_value()) {
        return 1;
    }

    xstate::snapshot now = xstate::get_initial_snapshot(*walk);
    for (const std::string_view type :
         {"speed up", "smells something", "carry on", "smells something", "start over"}) {
        const xstate::event happened{.type = std::string(type), .payload = {}};
        now = xstate::get_next_snapshot(*walk, now, happened);
        std::cout << type << " -> " << boost::json::serialize(now.value) << '\n';
    }
    // end::example[]
    return 0;
}
