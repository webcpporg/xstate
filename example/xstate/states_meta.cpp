// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    // tag::meta[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "prompt",
        "meta": {"title": "Feedback"},
        "states": {
            "prompt": {
                "description": "Waits for the user to rate the experience.",
                "meta": {"content": "How was your experience?"},
                "on": {"feedback.good": "thanks"}
            },
            "thanks": {"meta": {"content": "Thank you for your feedback!"}}
        }
    })");
    const xstate::result<xstate::machine> feedback = xstate::create_machine(config, {});
    if (!feedback.has_value()) {
        return 1;
    }
    xstate::snapshot now = xstate::get_initial_snapshot(*feedback);
    std::cout << xstate::get_meta(*feedback, now) << '\n';

    const xstate::event good{.type = "feedback.good", .payload = {}};
    now = xstate::get_next_snapshot(*feedback, now, good);
    std::cout << xstate::get_meta(*feedback, now) << '\n';

    const xstate::result<std::size_t> prompt = feedback->node_by_id("feedback.prompt");
    if (!prompt.has_value()) {
        return 1;
    }
    std::cout << feedback->node(*prompt).description.value_or("") << '\n';
    // end::meta[]
    return 0;
}
