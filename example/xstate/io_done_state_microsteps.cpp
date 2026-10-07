// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>

namespace xstate = webcpp::xstate;

int main() {
    // ({ event }) => ({ grams: event.grams }), from the event that entered the final state
    const xstate::value_maker dose_of_event =
        [](const xstate::action_args& args) -> xstate::result<std::optional<boost::json::value>> {
        const boost::json::value* grams = args.event.payload.if_contains("grams");
        if (grams == nullptr) {
            return xstate::failure<std::optional<boost::json::value>>(
                xstate::errc::implementation_failed);
        }
        return boost::json::value(boost::json::object{{"grams", *grams}});
    };
    // assign({ dose: ({ event }) => event.output })
    const xstate::assigner keep_dose =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::value* output = args.event.payload.if_contains("output");
        if (output == nullptr) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"dose", *output}};
    };

    xstate::implementations implementations;
    implementations.actions.emplace("keepDose", xstate::assign_action{.assignment = keep_dose});
    // The output of the final state whose id is coffee.preparation.ready.
    implementations.outputs.emplace("coffee.preparation.ready", dose_of_event);
    const boost::json::value config = boost::json::parse(R"({
        "id": "coffee",
        "context": {"dose": null},
        "initial": "preparation",
        "states": {
            "preparation": {
                "initial": "weighing",
                "states": {
                    "weighing": {"on": {"weighed": "grinding"}},
                    "grinding": {"on": {"ground": "ready"}},
                    "ready": {"type": "final"}
                },
                "onDone": {"target": "brewing", "actions": "keepDose"}
            },
            "brewing": {}
        }
    })");
    const xstate::result<xstate::machine> coffee_machine =
        xstate::create_machine(config, implementations);
    if (!coffee_machine.has_value()) {
        return 1;
    }

    const xstate::event weighed{.type = "weighed", .payload = {}};
    const xstate::event ground{.type = "ground", .payload = {{"grams", 18}}};
    xstate::snapshot snapshot = xstate::initial_transition(*coffee_machine).first;
    snapshot = xstate::transition(*coffee_machine, snapshot, weighed).first;
    // tag::microsteps[]
    for (const xstate::microstep& step :
         xstate::get_microsteps(*coffee_machine, snapshot, ground)) {
        std::cout << boost::json::serialize(xstate::to_json(step.event)) << " -> "
                  << boost::json::serialize(step.snapshot.value) << '\n';
    }
    // end::microsteps[]
    return 0;
}
