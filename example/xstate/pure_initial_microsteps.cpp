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
        "initial": "a",
        "states": {
            "a": {
                "entry": "enterA",
                "always": {"target": "b", "actions": "aToB"}
            },
            "b": {"entry": "enterB"}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }

    const std::vector<xstate::microstep> microsteps = xstate::get_initial_microsteps(*machine);
    for (const xstate::microstep& micro : microsteps) {
        std::cout << boost::json::serialize(micro.snapshot.value) << ' ' << types_of(micro.actions)
                  << '\n';
    }
    // end::example[]
    return 0;
}
