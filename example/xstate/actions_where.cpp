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
    // tag::where[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "question",
        "states": {
            "question": {
                "exit": "exitAction",
                "on": {
                    "feedback.good": {"target": "thanks", "actions": "track"}
                }
            },
            "thanks": {"entry": "showConfetti"}
        }
    })");
    const xstate::result<xstate::machine> feedback = xstate::create_machine(config, {});
    if (!feedback.has_value()) {
        return 1;
    }

    const xstate::snapshot question = xstate::get_initial_snapshot(*feedback);
    const auto [thanks, actions] =
        xstate::transition(*feedback, question, {.type = "feedback.good", .payload = {}});
    std::cout << "feedback.good:";
    for (const xstate::action& one : actions) {
        std::cout << ' ' << one.type;
    }
    std::cout << '\n';
    // end::where[]
    return 0;
}
