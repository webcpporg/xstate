// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    // tag::final[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "coffee",
        "initial": "preparation",
        "states": {
            "preparation": {
                "initial": "weighing",
                "states": {
                    "weighing": {"on": {"weighed": "grinding"}},
                    "grinding": {"on": {"ground": "ready"}},
                    "ready": {"type": "final"}
                },
                "onDone": "brewing"
            },
            "brewing": {}
        }
    })");
    const xstate::result<xstate::machine> coffee = xstate::create_machine(config, {});
    if (!coffee.has_value()) {
        return 1;
    }
    const xstate::event weighed{.type = "weighed", .payload = {}};
    const xstate::snapshot grinding =
        xstate::get_next_snapshot(*coffee, xstate::get_initial_snapshot(*coffee), weighed);
    std::cout << boost::json::serialize(grinding.value) << '\n';

    const xstate::event ground{.type = "ground", .payload = {}};
    for (const xstate::microstep& step : xstate::get_microsteps(*coffee, grinding, ground)) {
        std::cout << "microstep: " << boost::json::serialize(step.snapshot.value) << '\n';
    }
    // end::final[]
    return 0;
}
