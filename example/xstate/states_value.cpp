// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    // tag::value[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "question",
        "states": {
            "question": {"on": {"next": "form"}},
            "form": {
                "initial": "invalid",
                "states": {"invalid": {}, "valid": {}},
                "on": {"settings": "settings"}
            },
            "settings": {
                "type": "parallel",
                "states": {
                    "display": {"initial": "bright", "states": {"bright": {}, "dim": {}}},
                    "theme": {"initial": "dark", "states": {"dark": {}, "light": {}}}
                }
            }
        }
    })");
    const xstate::result<xstate::machine> feedback = xstate::create_machine(config, {});
    if (!feedback.has_value()) {
        return 1;
    }

    xstate::snapshot now = xstate::get_initial_snapshot(*feedback);
    std::cout << boost::json::serialize(now.value) << '\n';

    const xstate::event next{.type = "next", .payload = {}};
    now = xstate::get_next_snapshot(*feedback, now, next);
    std::cout << boost::json::serialize(now.value) << '\n';

    const xstate::event settings{.type = "settings", .payload = {}};
    now = xstate::get_next_snapshot(*feedback, now, settings);
    std::cout << boost::json::serialize(now.value) << '\n';

    const boost::json::value counting_config = boost::json::parse(R"({"id": "counting"})");
    const xstate::result<xstate::machine> counting = xstate::create_machine(counting_config, {});
    if (!counting.has_value()) {
        return 1;
    }
    std::cout << boost::json::serialize(xstate::get_initial_snapshot(*counting).value) << '\n';
    // end::value[]
    return 0;
}
