// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "light",
        "initial": "green",
        "context": {"cycles": 0},
        "states": {
            "green": {"on": {"TIMER": "yellow"}},
            "yellow": {"on": {"TIMER": "red"}},
            "red": {"on": {"TIMER": "green"}}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }
    const xstate::event timer{.type = "TIMER", .payload = {}};
    const xstate::snapshot current_state =
        xstate::get_next_snapshot(*machine, xstate::get_initial_snapshot(*machine), timer);

    const std::string state_to_persist = boost::json::serialize(boost::json::object{
        {"value", current_state.value},
        {"context", current_state.context},
    });

    /** Later, perhaps in another process, the persisted text is read back. */
    boost::system::error_code malformed;
    const boost::json::value persisted = boost::json::parse(state_to_persist, malformed);
    const boost::json::object* fields = persisted.if_object();
    if (malformed || fields == nullptr || !fields->contains("value") ||
        !fields->contains("context")) {
        return 1;
    }
    const xstate::result<xstate::snapshot> restored_state =
        xstate::resolve_state(*machine, fields->at("value"), fields->at("context"));
    if (!restored_state.has_value()) {
        return 1;
    }
    const auto [next_state, actions] = xstate::transition(*machine, *restored_state, timer);
    std::cout << "restored: " << boost::json::serialize(restored_state->value) << '\n';
    std::cout << "context: " << boost::json::serialize(restored_state->context) << '\n';
    std::cout << "next: " << boost::json::serialize(next_state.value) << '\n';
    // end::example[]
    return 0;
}
