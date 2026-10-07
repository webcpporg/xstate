// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

// tag::main[]
int main() {
    // 1. Arrange
    const boost::json::value config = boost::json::parse(R"({
        "initial": "inactive",
        "states": {
            "inactive": {"on": {"toggle": {"target": "active"}}},
            "active": {
                "entry": {"type": "notify", "params": {"message": "Active!"}},
                "on": {"toggle": {"target": "inactive"}}
            }
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }

    // 2. Act
    const xstate::event toggle{.type = "toggle", .payload = {}};
    xstate::snapshot current = xstate::get_initial_snapshot(*machine);
    boost::json::array notified_messages;
    for (int sent = 0; sent < 3; ++sent) {
        const auto [next, actions] = xstate::transition(*machine, current, toggle);
        for (const xstate::action& returned : actions) {
            const boost::json::value* message =
                returned.params.is_object() ? returned.params.get_object().if_contains("message")
                                            : nullptr;
            if (returned.type == "notify" && message != nullptr) {
                notified_messages.push_back(*message);
            }
        }
        current = next;
    }

    // 3. Assert
    const boost::json::value expected = boost::json::parse(R"(["Active!", "Active!"])");
    if (current.value != boost::json::value("active") ||
        boost::json::value(notified_messages) != expected) {
        return 1;
    }
    std::cout << "value: " << boost::json::serialize(current.value) << '\n';
    std::cout << "notified: " << boost::json::serialize(notified_messages) << '\n';
    return 0;
}

// end::main[]
