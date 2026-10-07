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
        "id": "feedback",
        "initial": "prompt",
        "states": {
            "prompt": {
                "on": {
                    "feedback.good": "thanks",
                    "feedback.*": "form",
                    "feedback.bad.*": "apology",
                    "*": "other"
                }
            },
            "thanks": {},
            "form": {},
            "apology": {},
            "other": {}
        }
    })");
    const xstate::result<xstate::machine> feedback = xstate::create_machine(config, {});
    if (!feedback.has_value()) {
        return 1;
    }

    const xstate::snapshot prompt = xstate::initial_transition(*feedback).first;
    for (const std::string_view type : {
             "feedback.good",
             "feedback.bad",
             "feedback.bad.rude",
             "feedback.meh",
             "feedback",
             "feedbacks",
             "mouse.click",
             "*",
         }) {
        const xstate::event happened{.type = std::string(type), .payload = {}};
        const xstate::snapshot next = xstate::transition(*feedback, prompt, happened).first;
        if (next.status == xstate::status::error) {
            std::cout << type << " -> " << next.error.message() << '\n';
            continue;
        }
        std::cout << type << " -> " << boost::json::serialize(next.value) << '\n';
    }
    // end::example[]
    return 0;
}
