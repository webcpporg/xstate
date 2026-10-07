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
#include <tuple>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

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

}  // namespace

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "machine",
        "initial": "parentState",
        "states": {
            "parentState": {
                "entry": "enterParent",
                "exit": "exitParent",
                "initial": "someChildState",
                "on": {
                    "event.targetless": {"actions": "notify"},
                    "event.normal": {"target": ".someChildState"},
                    "event.thatReenters": {"target": ".otherChildState", "reenter": true},
                    "event.self": {"target": "parentState"},
                    "event.selfReenters": {"target": "parentState", "reenter": true}
                },
                "states": {
                    "someChildState": {"entry": "enterSome", "exit": "exitSome"},
                    "otherChildState": {"entry": "enterOther", "exit": "exitOther"}
                }
            }
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }

    auto [now, actions] = xstate::initial_transition(*machine);
    std::cout << "start " << boost::json::serialize(now.value) << ' ' << types_of(actions) << '\n';
    for (const std::string_view type : {
             "event.thatReenters",
             "event.targetless",
             "event.self",
             "event.normal",
             "event.selfReenters",
         }) {
        const xstate::event happened{.type = std::string(type), .payload = {}};
        std::tie(now, actions) = xstate::transition(*machine, now, happened);
        std::cout << type << " -> " << boost::json::serialize(now.value) << ' ' << types_of(actions)
                  << '\n';
    }
    // end::example[]
    return 0;
}
