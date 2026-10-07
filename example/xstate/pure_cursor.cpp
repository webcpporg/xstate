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

// tag::helpers[]
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

/** The transitions a microstep took, as source -> targets, or "none". */
std::string transitions_of(const xstate::machine& owner, const xstate::microstep& micro) {
    if (!micro.transitions.has_value() || micro.transitions->empty()) {
        return "none";
    }
    std::string listed;
    for (const xstate::transition_definition* one : *micro.transitions) {
        if (!listed.empty()) {
            listed += ", ";
        }
        listed += owner.node(one->source).key + " ->";
        for (const std::size_t target : one->target.value_or(std::vector<std::size_t>{})) {
            listed += ' ' + owner.node(target).key;
        }
    }
    return listed;
}

// end::helpers[]

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

    // tag::machine[]
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
    // end::machine[]
    const xstate::result<xstate::machine> order = xstate::create_machine(config, implementations);
    if (!order.has_value()) {
        return 1;
    }
    const xstate::snapshot idle = xstate::get_initial_snapshot(*order);

    // tag::example[]
    xstate::macrostep cursor =
        xstate::begin(*order, idle, xstate::event{.type = "CHECKOUT", .payload = {}});
    std::size_t count = 0;
    while (!cursor.done()) {
        const xstate::progress made = cursor.next();
        const xstate::microstep& micro = made.step;
        std::cout << ++count << ": " << micro.event.type << ", " << transitions_of(*order, micro)
                  << ", " << types_of(micro.actions) << ", "
                  << boost::json::serialize(micro.snapshot.value)
                  << (made.settled ? ", settled" : "") << '\n';
    }
    const xstate::macrostep_result& settled = cursor.result();
    std::cout << "result: " << boost::json::serialize(settled.snapshot.value) << ", "
              << types_of(settled.actions) << ", " << settled.microsteps << " microsteps\n";
    // end::example[]
    return 0;
}
