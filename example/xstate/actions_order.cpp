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
    // tag::order[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "question",
        "entry": "rootEntry",
        "exit": "rootExit",
        "states": {
            "question": {
                "initial": "asking",
                "entry": "enterQuestion",
                "exit": "exitQuestion",
                "states": {
                    "asking": {
                        "entry": "enterAsking",
                        "exit": "exitAsking",
                        "on": {
                            "feedback.good": {"target": "#feedback.thanks", "actions": "track"}
                        }
                    }
                }
            },
            "thanks": {"type": "final", "entry": "showConfetti", "exit": "exitThanks"}
        }
    })");
    const xstate::result<xstate::machine> feedback = xstate::create_machine(config, {});
    if (!feedback.has_value()) {
        return 1;
    }

    const auto [initial, initial_actions] = xstate::initial_transition(*feedback);
    std::cout << "initial:";
    for (const xstate::action& one : initial_actions) {
        std::cout << ' ' << one.type;
    }
    std::cout << '\n';

    const auto [next, actions] =
        xstate::transition(*feedback, initial, {.type = "feedback.good", .payload = {}});
    std::cout << "feedback.good:";
    for (const xstate::action& one : actions) {
        std::cout << ' ' << one.type;
    }
    std::cout << '\n';
    // end::order[]
    return 0;
}
