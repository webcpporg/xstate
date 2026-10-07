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

namespace {

// tag::print[]
/** Prints a snapshot's state value, then the id of each active node, in order. */
void print_nodes(const xstate::machine& owner, const xstate::snapshot& of) {
    std::cout << boost::json::serialize(of.value) << '\n';
    for (const std::size_t node : of.nodes) {
        std::cout << ' ' << owner.node(node).id;
    }
    std::cout << '\n';
}

// end::print[]

}  // namespace

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "player",
        "initial": "on",
        "states": {
            "off": {},
            "on": {
                "type": "parallel",
                "on": {"OFF": "#player.off"},
                "states": {
                    "track": {
                        "initial": "paused",
                        "states": {
                            "paused": {"on": {"PLAY": "playing"}},
                            "playing": {}
                        }
                    },
                    "volume": {
                        "initial": "normal",
                        "states": {
                            "normal": {"on": {"MUTE": "muted"}},
                            "muted": {}
                        }
                    }
                }
            }
        }
    })");
    const xstate::result<xstate::machine> player = xstate::create_machine(config, {});
    if (!player.has_value()) {
        return 1;
    }

    const xstate::snapshot paused = xstate::get_initial_snapshot(*player);
    print_nodes(*player, paused);
    const xstate::snapshot playing =
        xstate::get_next_snapshot(*player, paused, xstate::event{.type = "PLAY", .payload = {}});
    print_nodes(*player, playing);
    // end::example[]
    return 0;
}
