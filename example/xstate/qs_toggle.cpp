// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// tag::program[]
// tag::includes[]
#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;
// end::includes[]

int main() {
    // tag::machine[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "toggle",
        "initial": "Inactive",
        "states": {
            "Inactive": {"on": {"toggle": "Active"}},
            "Active": {"on": {"toggle": "Inactive"}}
        }
    })");
    const xstate::result<xstate::machine> toggle_machine = xstate::create_machine(config, {});
    if (!toggle_machine.has_value()) {
        std::cout << "refused: " << toggle_machine.error().message() << '\n';
        return 1;
    }
    // end::machine[]

    // tag::pure[]
    const auto [initial_state, initial_actions] = xstate::initial_transition(*toggle_machine);
    std::cout << "Value: " << boost::json::serialize(initial_state.value) << " ("
              << initial_actions.size() << " actions)\n";

    const xstate::event toggle{.type = "toggle", .payload = {}};
    const auto [next_state, actions] = xstate::transition(*toggle_machine, initial_state, toggle);
    std::cout << "Value: " << boost::json::serialize(next_state.value) << " (" << actions.size()
              << " actions)\n";

    const auto [last_state, last_actions] = xstate::transition(*toggle_machine, next_state, toggle);
    std::cout << "Value: " << boost::json::serialize(last_state.value) << " ("
              << last_actions.size() << " actions)\n";
    // end::pure[]
    return 0;
}

// end::program[]
