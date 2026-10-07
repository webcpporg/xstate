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
    // tag::reload[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "toggle",
        "version": "1.0",
        "initial": "inactive",
        "context": {"presses": 0},
        "states": {
            "inactive": {"on": {"toggle": "active"}},
            "active": {"on": {"toggle": "inactive"}}
        }
    })");
    const xstate::result<xstate::machine> toggle = xstate::create_machine(config, {});
    if (!toggle.has_value()) {
        return 1;
    }
    const std::string stored = boost::json::serialize(xstate::to_json(*toggle));

    boost::system::error_code malformed;
    const boost::json::value definition = boost::json::parse(stored, malformed);
    if (malformed) {
        return 1;
    }
    const xstate::result<xstate::machine> reloaded = xstate::create_machine_from_definition(
        definition, xstate::implementations(), toggle->context());
    if (!reloaded.has_value()) {
        return 1;
    }

    const xstate::snapshot initial = xstate::initial_transition(*reloaded).first;
    const xstate::event pressed{.type = "toggle", .payload = {}};
    const xstate::snapshot next = xstate::transition(*reloaded, initial, pressed).first;
    std::cout << boost::json::serialize(next.value) << ' ' << boost::json::serialize(next.context)
              << '\n';
    std::cout << std::boolalpha << (xstate::to_json(*reloaded) == definition) << '\n';
    // end::reload[]
    return 0;
}
