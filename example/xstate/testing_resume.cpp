// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
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

// tag::helpers[]
/** Runs the microsteps a cursor has left; the macrostep it settles. */
xstate::macrostep_result finish(xstate::macrostep& cursor) {
    while (!cursor.done()) {
        cursor.next();
    }
    return cursor.result();
}

/** Whether two settled macrosteps agree: microsteps, snapshot and actions. */
bool same(const xstate::macrostep_result& left, const xstate::macrostep_result& right) {
    return left.microsteps == right.microsteps && left.snapshot.value == right.snapshot.value &&
           left.snapshot.context == right.snapshot.context &&
           types_of(left.actions) == types_of(right.actions);
}

// end::helpers[]

}  // namespace

int main() {
    const boost::json::value config = boost::json::parse(R"({
        "initial": "idle",
        "states": {
            "idle": {"on": {"GO": "one"}},
            "one": {"entry": "first", "always": "two"},
            "two": {"entry": "second", "always": "three"},
            "three": {"entry": "third"}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }
    const xstate::snapshot idle = xstate::get_initial_snapshot(*machine);
    const xstate::event go{.type = "GO", .payload = {}};

    // tag::test[]
    xstate::macrostep straight = xstate::begin(*machine, idle, go);
    const xstate::macrostep_result expected = finish(straight);

    for (std::size_t stopped = 0; stopped <= expected.microsteps; ++stopped) {
        xstate::macrostep original = xstate::begin(*machine, idle, go);
        for (std::size_t paid = 0; paid < stopped; ++paid) {
            original.next();
        }
        xstate::macrostep copy = original;
        const xstate::macrostep_result from_copy = finish(copy);
        const xstate::macrostep_result from_original = finish(original);
        if (!same(from_copy, expected) || !same(from_original, expected)) {
            return 1;
        }
        std::cout << "stopped after " << stopped << ", the cursor and its copy settle at "
                  << boost::json::serialize(from_copy.snapshot.value) << " with "
                  << types_of(from_copy.actions) << '\n';
    }
    // end::test[]
    return 0;
}
