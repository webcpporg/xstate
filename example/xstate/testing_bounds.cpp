// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <iostream>
#include <optional>
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

// tag::bound[]
/**
 Runs at most `fuel` microsteps of a macrostep: its result once it has
 settled, nothing when the fuel ran out first, the cursor left where it
 stopped for its caller to resume or drop.
*/
std::optional<xstate::macrostep_result> settle_within(xstate::macrostep& step, std::size_t fuel) {
    for (std::size_t paid = 0; paid < fuel && !step.done(); ++paid) {
        step.next();
    }
    if (!step.done()) {
        return std::nullopt;
    }
    return step.result();
}

// end::bound[]

}  // namespace

// tag::main[]
int main() {
    const boost::json::value config = boost::json::parse(R"({
        "initial": "idle",
        "states": {
            "idle": {"on": {"GO": "one", "SPIN": "ping"}},
            "one": {"entry": "first", "always": "two"},
            "two": {"entry": "second", "always": "three"},
            "three": {"entry": "third"},
            "ping": {"always": "pong"},
            "pong": {"always": "ping"}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }
    const xstate::snapshot idle = xstate::get_initial_snapshot(*machine);

    const xstate::event go{.type = "GO", .payload = {}};
    xstate::macrostep going = xstate::begin(*machine, idle, go);
    const std::optional<xstate::macrostep_result> went = settle_within(going, 10);
    if (!went.has_value() || went->snapshot.value != boost::json::value("three")) {
        return 1;
    }
    std::cout << "GO settles in " << went->microsteps << " microsteps at "
              << boost::json::serialize(went->snapshot.value) << " with " << types_of(went->actions)
              << '\n';

    const xstate::event spin{.type = "SPIN", .payload = {}};
    xstate::macrostep spinning = xstate::begin(*machine, idle, spin);
    if (settle_within(spinning, 1000).has_value()) {
        return 1;
    }
    std::cout << "SPIN has not settled after 1000 microsteps\n";
    return 0;
}

// end::main[]
