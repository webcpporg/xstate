// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

// tag::types[]
/** The types of `actions`, as a list: [a, b]. */
std::string types_of(const std::vector<xstate::action>& actions) {
    std::string listed = "[";
    for (const xstate::action& one : actions) {
        if (listed.size() > 1) {
            listed += ", ";
        }
        listed += one.type;
    }
    return listed + "]";
}

// end::types[]

}  // namespace

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "initial": "pending",
        "states": {
            "pending": {"on": {"start": {"target": "started"}}},
            "started": {"entry": {"type": "doSomething"}}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }

    const auto [initial_state, initial_actions] = xstate::initial_transition(*machine);
    std::cout << "initial value: " << boost::json::serialize(initial_state.value) << '\n';
    std::cout << "initial actions: " << types_of(initial_actions) << '\n';

    const xstate::event start{.type = "start", .payload = {}};
    const auto [next_state, actions] = xstate::transition(*machine, initial_state, start);
    std::cout << "next value: " << boost::json::serialize(next_state.value) << '\n';
    std::cout << "actions: " << types_of(actions) << '\n';
    // end::example[]
    return 0;
}
