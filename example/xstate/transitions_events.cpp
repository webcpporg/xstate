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
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "question",
        "states": {
            "question": {
                "on": {
                    "feedback.good": {"target": "thanks"}
                }
            },
            "thanks": {}
        }
    })");
    const xstate::result<xstate::machine> feedback = xstate::create_machine(config, {});
    if (!feedback.has_value()) {
        return 1;
    }

    const xstate::result<xstate::event> event = xstate::event_from_json(boost::json::parse(
        R"({"type": "feedback.good", "feedback": "This is great!", "rating": 5})"));
    if (!event.has_value()) {
        return 1;
    }
    std::cout << event->type << ' ' << boost::json::serialize(event->payload) << '\n';

    const xstate::snapshot question = xstate::initial_transition(*feedback).first;
    const xstate::snapshot thanks = xstate::transition(*feedback, question, *event).first;
    std::cout << boost::json::serialize(question.value) << " -> "
              << boost::json::serialize(thanks.value) << '\n';
    // end::example[]
    return 0;
}
