// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

// tag::status[]
/** The name of a status, as XState spells it. */
std::string_view name_of(xstate::status status) {
    switch (status) {
        case xstate::status::active: return "active";
        case xstate::status::done: return "done";
        case xstate::status::error: return "error";
    }
    return "unknown";
}

// end::status[]

}  // namespace

int main() {
    // tag::example[]
    xstate::implementations implementations;
    implementations.context =
        [](const boost::json::value& input) -> xstate::result<boost::json::value> {
        const boost::json::object* given = input.if_object();
        const boost::json::value* count =
            given == nullptr ? nullptr : given->if_contains("initialCount");
        if (count == nullptr) {
            return xstate::failure<boost::json::value>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"count", *count}};
    };
    const boost::json::value config = boost::json::parse(R"({
        "initial": "pending",
        "states": {
            "pending": {"on": {"start": {"target": "started"}}},
            "started": {"entry": {"type": "doSomething"}}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, implementations);
    if (!machine.has_value()) {
        return 1;
    }

    const boost::json::value input = boost::json::object{{"initialCount", 0}};
    const auto [initial_state, initial_actions] = xstate::initial_transition(*machine, input);
    std::cout << "value: " << boost::json::serialize(initial_state.value) << '\n';
    std::cout << "context: " << boost::json::serialize(initial_state.context) << '\n';
    std::cout << "status: " << name_of(initial_state.status) << '\n';

    const auto [without_input, no_actions] = xstate::initial_transition(*machine);
    std::cout << "without input: " << name_of(without_input.status) << ", context "
              << boost::json::serialize(without_input.context) << '\n';
    // end::example[]
    return 0;
}
