// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

namespace {

// tag::implementation[]
/** Keeps the event's age, and fails when the event carries none. */
xstate::result<boost::json::object> save_age(const xstate::action_args& args) {
    const boost::json::value* age = args.event.payload.if_contains("age");
    if (age == nullptr || !age->is_number()) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"age", *age}};
}

// end::implementation[]

}  // namespace

// tag::main[]
int main() {
    // 1. Arrange
    xstate::implementations implementations;
    implementations.actions.emplace("saveAge", xstate::assign_action{.assignment = save_age});
    const boost::json::value config = boost::json::parse(R"({
        "initial": "editing",
        "context": {"age": 0},
        "states": {
            "editing": {"on": {"save": {"target": "saved", "actions": "saveAge"}}},
            "saved": {}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, implementations);
    if (!machine.has_value()) {
        return 1;
    }

    // 2. Act
    const xstate::snapshot editing = xstate::get_initial_snapshot(*machine);
    const xstate::event save{.type = "save", .payload = {}};
    const xstate::snapshot failed = xstate::get_next_snapshot(*machine, editing, save);

    // 3. Assert
    if (failed.status != xstate::status::error ||
        failed.error != xstate::errc::implementation_failed ||
        failed.value != boost::json::value("editing")) {
        return 1;
    }
    std::cout << "status: error\n";
    std::cout << "value: " << boost::json::serialize(failed.value) << '\n';
    std::cout << "context: " << boost::json::serialize(failed.context) << '\n';
    return 0;
}

// end::main[]
