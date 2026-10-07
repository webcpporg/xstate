// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string>
#include <string_view>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "machine",
        "initial": "a",
        "on": {"reset": ".a"},
        "states": {
            "a": {"on": {"next": "b", "deep": "b.two"}},
            "b": {
                "initial": "one",
                "on": {"next": "#c"},
                "states": {"one": {}, "two": {}}
            },
            "c": {
                "id": "c",
                "initial": "child",
                "on": {"next": ".other"},
                "states": {"child": {}, "other": {}}
            }
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }

    xstate::snapshot now = xstate::initial_transition(*machine).first;
    std::cout << "start " << boost::json::serialize(now.value) << '\n';
    for (const std::string_view type : {"next", "next", "next", "reset", "deep"}) {
        const xstate::event happened{.type = std::string(type), .payload = {}};
        now = xstate::transition(*machine, now, happened).first;
        std::cout << type << " -> " << boost::json::serialize(now.value) << '\n';
    }
    // end::example[]
    return 0;
}
