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
    // tag::finite[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "context": {"name": "", "email": "", "feedback": ""},
        "initial": "prompt",
        "states": {
            "prompt": {"on": {"feedback.good": "thanks", "feedback.bad": "form"}},
            "form": {"on": {"feedback.submit": "thanks"}},
            "thanks": {"on": {"feedback.close": "closed"}},
            "closed": {"type": "final"}
        }
    })");
    const xstate::result<xstate::machine> feedback = xstate::create_machine(config, {});
    if (!feedback.has_value()) {
        return 1;
    }

    const xstate::snapshot first = xstate::get_initial_snapshot(*feedback);
    std::cout << boost::json::serialize(first.value) << '\n';  // the finite state
    std::cout << boost::json::serialize(first.context) << '\n';

    const xstate::event good{.type = "feedback.good", .payload = {}};
    const xstate::snapshot next = xstate::get_next_snapshot(*feedback, first, good);
    std::cout << boost::json::serialize(next.value) << '\n';
    // end::finite[]
    return 0;
}
