// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    // tag::lazy[]
    /** The caller's clock, in milliseconds: xstate reads none. */
    std::uint64_t now = 1000;
    xstate::implementations implementations;
    implementations.context =
        [&now](const boost::json::value& /*input*/) -> xstate::result<boost::json::value> {
        return boost::json::object{{"feedback", "Some feedback"}, {"createdAt", now}};
    };
    const boost::json::value config = boost::json::parse(R"({"id": "feedback"})");
    const xstate::result<xstate::machine> feedback_machine =
        xstate::create_machine(config, implementations);
    if (!feedback_machine.has_value()) {
        return 1;
    }

    now = 2000;
    const xstate::snapshot first = xstate::get_initial_snapshot(*feedback_machine);
    now = 3000;
    const xstate::snapshot second = xstate::get_initial_snapshot(*feedback_machine);
    std::cout << boost::json::serialize(first.context) << '\n'
              << boost::json::serialize(second.context) << '\n';
    // end::lazy[]
    return 0;
}
