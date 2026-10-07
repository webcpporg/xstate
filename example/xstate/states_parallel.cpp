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

int main() {
    // tag::parallel[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "player",
        "type": "parallel",
        "states": {
            "track": {
                "initial": "paused",
                "states": {
                    "paused": {"on": {"PLAY": "playing"}},
                    "playing": {"on": {"STOP": "paused", "RESET": "paused"}}
                }
            },
            "volume": {
                "initial": "normal",
                "states": {
                    "normal": {"on": {"MUTE": "muted"}},
                    "muted": {"on": {"UNMUTE": "normal", "RESET": "normal"}}
                }
            }
        }
    })");
    const xstate::result<xstate::machine> player = xstate::create_machine(config, {});
    if (!player.has_value()) {
        return 1;
    }
    xstate::snapshot now = xstate::get_initial_snapshot(*player);
    std::cout << boost::json::serialize(now.value) << '\n';

    now = xstate::get_next_snapshot(*player, now, xstate::event{.type = "PLAY", .payload = {}});
    now = xstate::get_next_snapshot(*player, now, xstate::event{.type = "MUTE", .payload = {}});
    std::cout << boost::json::serialize(now.value) << '\n';

    const xstate::event reset{.type = "RESET", .payload = {}};
    const std::vector<xstate::microstep> steps = xstate::get_microsteps(*player, now, reset);
    if (steps.empty()) {
        return 1;
    }
    const xstate::snapshot& after = steps.back().snapshot;
    std::cout << steps.size() << " microstep: " << boost::json::serialize(after.value) << '\n';
    // end::parallel[]
    return 0;
}
