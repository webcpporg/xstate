// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>
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
    const xstate::event_maker raised =
        [](const xstate::action_args&) -> xstate::result<xstate::event> {
        return xstate::event{.type = "RAISED", .payload = {}};
    };
    xstate::implementations implementations;
    implementations.actions.emplace(
        "raiseRaised",
        xstate::raise_action{.event = raised, .id = std::nullopt, .delay = std::nullopt});

    const boost::json::value config = boost::json::parse(R"({
        "initial": "first",
        "states": {
            "first": {
                "on": {"TRIGGER": {"target": "second", "actions": "raiseRaised"}}
            },
            "second": {"on": {"RAISED": "third"}},
            "third": {"always": "fourth"},
            "fourth": {}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, implementations);
    if (!machine.has_value()) {
        return 1;
    }

    const xstate::snapshot initial_state = xstate::get_initial_snapshot(*machine);
    const xstate::event trigger{.type = "TRIGGER", .payload = {}};
    for (const xstate::microstep& micro :
         xstate::get_microsteps(*machine, initial_state, trigger)) {
        std::cout << boost::json::serialize(micro.snapshot.value) << ' ' << types_of(micro.actions)
                  << '\n';
    }
    // end::example[]
    return 0;
}
