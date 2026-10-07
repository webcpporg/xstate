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
        "id": "settings",
        "type": "parallel",
        "on": {
            "set.dark.custom": {"target": [".mode.dark", ".theme.custom"]},
            "set.light": {"target": ".mode.light"}
        },
        "states": {
            "mode": {
                "initial": "light",
                "on": {"mode.light": ".light"},
                "states": {
                    "light": {"on": {"toggle": "dark"}},
                    "dark": {"on": {"toggle": "light"}}
                }
            },
            "theme": {
                "initial": "default",
                "states": {
                    "default": {"on": {"change": "custom"}},
                    "custom": {"on": {"change": "default"}}
                }
            }
        }
    })");
    const xstate::result<xstate::machine> settings = xstate::create_machine(config, {});
    if (!settings.has_value()) {
        return 1;
    }

    xstate::snapshot now = xstate::initial_transition(*settings).first;
    std::cout << "start " << boost::json::serialize(now.value) << '\n';
    for (const std::string_view type :
         {"set.dark.custom", "set.light", "set.dark.custom", "mode.light"}) {
        const xstate::event happened{.type = std::string(type), .payload = {}};
        now = xstate::transition(*settings, now, happened).first;
        std::cout << type << " -> " << boost::json::serialize(now.value) << '\n';
    }
    // end::example[]
    return 0;
}
