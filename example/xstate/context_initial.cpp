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
    // tag::initial[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "context": {"feedback": "Some feedback", "rating": 5}
    })");
    const xstate::result<xstate::machine> feedback_machine = xstate::create_machine(config, {});
    if (!feedback_machine.has_value()) {
        return 1;
    }
    const xstate::snapshot snapshot = xstate::get_initial_snapshot(*feedback_machine);
    std::cout << boost::json::serialize(snapshot.context) << '\n';
    // end::initial[]

    // tag::none[]
    const boost::json::value no_context = boost::json::parse(R"({"id": "toggle"})");
    const xstate::result<xstate::machine> toggle = xstate::create_machine(no_context, {});
    if (!toggle.has_value()) {
        return 1;
    }
    std::cout << boost::json::serialize(xstate::get_initial_snapshot(*toggle).context) << '\n';
    // end::none[]
    return 0;
}
