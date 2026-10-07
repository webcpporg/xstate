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

// tag::main[]
int main() {
    const boost::json::value config = boost::json::parse(R"({
        "initial": "idle",
        "states": {
            "idle": {"on": {"submit": "validating"}},
            "validating": {"entry": "validate", "always": "submitted"},
            "submitted": {}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }

    const xstate::snapshot initial = xstate::get_initial_snapshot(*machine);
    const xstate::event submit{.type = "submit", .payload = {}};
    xstate::macrostep step = xstate::begin(*machine, initial, submit);
    std::vector<boost::json::value> values;
    while (!step.done()) {
        const xstate::progress made = step.next();
        values.push_back(made.step.snapshot.value);
        std::cout << boost::json::serialize(made.step.snapshot.value) << ' '
                  << types_of(made.step.actions) << (made.settled ? " settled" : "") << '\n';
    }

    // The transient state was entered, then left in the same macrostep.
    const std::vector<boost::json::value> expected{"validating", "submitted"};
    if (values != expected) {
        return 1;
    }
    return 0;
}

// end::main[]
