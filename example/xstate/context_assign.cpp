// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

namespace {

// tag::assigner[]
xstate::result<boost::json::object> update_feedback(const xstate::action_args& args) {
    const boost::json::value* feedback = args.event.payload.if_contains("feedback");
    if (feedback == nullptr) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"feedback", *feedback}};
}

// end::assigner[]
}  // namespace

int main() {
    // tag::assign[]
    xstate::implementations implementations;
    implementations.actions.emplace("updateFeedback",
                                    xstate::assign_action{.assignment = update_feedback});
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "context": {"feedback": "Some feedback", "rating": 5},
        "on": {"feedback.update": {"actions": "updateFeedback"}}
    })");
    const xstate::result<xstate::machine> feedback_machine =
        xstate::create_machine(config, implementations);
    if (!feedback_machine.has_value()) {
        return 1;
    }

    const xstate::snapshot initial = xstate::get_initial_snapshot(*feedback_machine);
    // tag::event[]
    const xstate::event update{
        .type = "feedback.update",
        .payload = {{"feedback", "Some other feedback"}},
    };
    const auto [next, actions] = xstate::transition(*feedback_machine, initial, update);
    // end::event[]
    std::cout << boost::json::serialize(initial.context) << '\n'
              << boost::json::serialize(next.context) << '\n'
              << actions.size() << " actions\n";
    // end::assign[]
    return 0;
}
