// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

/** The context a feedback machine starts with, read from its input. */
xstate::result<boost::json::value> feedback_context(const boost::json::value& input) {
    const boost::json::object* given = input.if_object();
    const boost::json::value* user_id = given == nullptr ? nullptr : given->if_contains("userId");
    const boost::json::value* rating =
        given == nullptr ? nullptr : given->if_contains("defaultRating");
    if (user_id == nullptr || rating == nullptr) {
        return xstate::failure<boost::json::value>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"userId", *user_id}, {"feedback", ""}, {"rating", *rating}};
}

}  // namespace

int main() {
    xstate::implementations implementations;
    implementations.context = feedback_context;
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "prompt",
        "states": {
            "prompt": {}
        }
    })");
    const xstate::result<xstate::machine> feedback_machine =
        xstate::create_machine(config, implementations);
    if (!feedback_machine.has_value()) {
        return 1;
    }

    // tag::pure[]
    const boost::json::object input{{"userId", "123"}, {"defaultRating", 5}};

    const xstate::snapshot started = xstate::initial_transition(*feedback_machine, input).first;
    std::cout << boost::json::serialize(started.context) << '\n';

    const std::vector<xstate::microstep> microsteps =
        xstate::get_initial_microsteps(*feedback_machine, input);
    if (microsteps.empty()) {
        return 1;
    }
    std::cout << boost::json::serialize(microsteps.back().snapshot.context) << " after "
              << microsteps.size() << " microstep\n";
    // end::pure[]
    return 0;
}
