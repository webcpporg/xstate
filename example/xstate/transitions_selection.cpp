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
#include <vector>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "editor",
        "initial": "editing",
        "states": {
            "editing": {
                "initial": "text",
                "on": {"escape": "closed"},
                "states": {
                    "text": {"on": {"menu.open": "menu", "save": "saving"}},
                    "menu": {"on": {"escape": "text"}},
                    "saving": {"on": {"escape": {}}}
                }
            },
            "closed": {}
        }
    })");
    const xstate::result<xstate::machine> editor = xstate::create_machine(config, {});
    if (!editor.has_value()) {
        return 1;
    }

    const std::vector<std::vector<std::string_view>> runs{
        {"menu.open", "escape", "escape"},
        {"save", "escape"},
    };
    for (const std::vector<std::string_view>& run : runs) {
        xstate::snapshot now = xstate::initial_transition(*editor).first;
        std::cout << "start " << boost::json::serialize(now.value) << '\n';
        for (const std::string_view type : run) {
            const xstate::event happened{.type = std::string(type), .payload = {}};
            now = xstate::transition(*editor, now, happened).first;
            std::cout << type << " -> " << boost::json::serialize(now.value) << '\n';
        }
    }
    // end::example[]
    return 0;
}
