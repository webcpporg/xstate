// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <iostream>
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

// tag::type_name[]
std::string_view type_name(xstate::node_type type) {
    switch (type) {
        case xstate::node_type::atomic: return "atomic";
        case xstate::node_type::compound: return "compound";
        case xstate::node_type::parallel: return "parallel";
        case xstate::node_type::final: return "final";
        case xstate::node_type::history: return "history";
    }
    return "unknown";
}

// end::type_name[]

}  // namespace

int main() {
    // tag::nodes[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "player",
        "initial": "stopped",
        "states": {
            "stopped": {"on": {"play": "playing"}},
            "playing": {
                "id": "active",
                "initial": "normal",
                "states": {"normal": {}, "fast": {}},
                "on": {"stop": "stopped"}
            },
            "done": {"type": "final"}
        }
    })");
    const xstate::result<xstate::machine> player = xstate::create_machine(config, {});
    if (!player.has_value()) {
        return 1;
    }

    for (std::size_t index = 0; index < player->size(); ++index) {
        const xstate::state_node& node = player->node(index);
        std::cout << index << ' ' << node.key << ' ' << node.id << ' ' << type_name(node.type)
                  << '\n';
    }

    const xstate::result<std::size_t> active = player->node_by_id("active");
    if (!active.has_value()) {
        return 1;
    }
    const xstate::result<std::size_t> fast = player->child(*active, "fast");
    if (!fast.has_value()) {
        return 1;
    }
    const xstate::result<std::size_t> found = player->node_by_id("#active.fast");
    if (!found.has_value()) {
        return 1;
    }
    std::cout << player->node(*fast).id << ' ' << player->node(*found).id << '\n';
    // end::nodes[]
    return 0;
}
