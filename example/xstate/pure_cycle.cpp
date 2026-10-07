// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "spinner",
        "initial": "idle",
        "states": {
            "idle": {"on": {"SPIN": "left"}},
            "left": {"always": "right"},
            "right": {"always": "left"}
        }
    })");
    const xstate::result<xstate::machine> spinner = xstate::create_machine(config, {});
    if (!spinner.has_value()) {
        return 1;
    }
    const xstate::snapshot idle = xstate::get_initial_snapshot(*spinner);

    constexpr std::size_t fuel = 1000;
    xstate::macrostep cursor =
        xstate::begin(*spinner, idle, xstate::event{.type = "SPIN", .payload = {}});
    std::size_t spent = 0;
    while (!cursor.done() && spent < fuel) {
        cursor.next();
        ++spent;
    }
    if (!cursor.done()) {
        std::cout << "out of fuel after " << spent << " microsteps, at "
                  << boost::json::serialize(cursor.current().value) << '\n';
        std::cout << "the stable state is still "
                  << boost::json::serialize(cursor.began_from().value) << '\n';
    }
    // end::example[]
    return 0;
}
