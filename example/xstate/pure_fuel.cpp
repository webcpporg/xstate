// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <ios>
#include <iostream>
#include <optional>

namespace xstate = webcpp::xstate;

namespace {

// tag::run[]
/** Runs at most `fuel` microsteps of `cursor`, one unit each; returns the units spent. */
std::size_t run(xstate::macrostep& cursor, std::size_t fuel) {
    std::size_t spent = 0;
    while (!cursor.done() && spent < fuel) {
        cursor.next();
        ++spent;
    }
    return spent;
}

// end::run[]

}  // namespace

int main() {
    const xstate::event_maker charged =
        [](const xstate::action_args&) -> xstate::result<xstate::event> {
        return xstate::event{.type = "CHARGED", .payload = {}};
    };
    xstate::implementations implementations;
    implementations.actions.emplace(
        "charge",
        xstate::raise_action{.event = charged, .id = std::nullopt, .delay = std::nullopt});
    const boost::json::value config = boost::json::parse(R"({
        "id": "order",
        "initial": "idle",
        "states": {
            "idle": {"on": {"CHECKOUT": {"target": "validating", "actions": "track"}}},
            "validating": {"always": "charging"},
            "charging": {"entry": "charge", "on": {"CHARGED": "confirmed"}},
            "confirmed": {"entry": "sendReceipt"}
        }
    })");
    const xstate::result<xstate::machine> order = xstate::create_machine(config, implementations);
    if (!order.has_value()) {
        return 1;
    }
    const xstate::snapshot idle = xstate::get_initial_snapshot(*order);
    std::cout << std::boolalpha;

    // tag::example[]
    xstate::macrostep cursor =
        xstate::begin(*order, idle, xstate::event{.type = "CHECKOUT", .payload = {}});

    const std::size_t first = run(cursor, 2);
    std::cout << "first request: " << first << " microsteps, done " << cursor.done() << ", at "
              << boost::json::serialize(cursor.current().value) << '\n';

    xstate::macrostep copy = cursor;

    const std::size_t second = run(cursor, 2);
    std::cout << "second request: " << second << " microstep, done " << cursor.done() << ", at "
              << boost::json::serialize(cursor.result().snapshot.value) << '\n';

    run(copy, 2);
    std::cout << "the copy: done " << copy.done() << ", at "
              << boost::json::serialize(copy.result().snapshot.value) << ", "
              << copy.result().microsteps << " microsteps\n";
    // end::example[]
    return 0;
}
