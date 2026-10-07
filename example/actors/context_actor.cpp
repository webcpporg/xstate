// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

namespace {

xstate::result<boost::json::object> update_feedback(const xstate::action_args& args) {
    const boost::json::value* feedback = args.event.payload.if_contains("feedback");
    if (feedback == nullptr) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"feedback", *feedback}};
}

}  // namespace

int main() {
    // tag::actor[]
    xstate::implementations implementations;
    implementations.actions.emplace("updateFeedback",
                                    xstate::assign_action{.assignment = update_feedback});
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "context": {"feedback": "Some feedback"},
        "on": {"feedback.update": {"actions": "updateFeedback"}}
    })");
    const xstate::result<xstate::machine> feedback_machine =
        xstate::create_machine(config, implementations);
    if (!feedback_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> feedback_actor = system.create_actor(*feedback_machine);
    if (!feedback_actor.has_value()) {
        return 1;
    }
    const xstate::result<void> subscribed =
        system.subscribe(*feedback_actor, [](const xstate::snapshot& state) {
            std::cout << boost::json::serialize(state.context) << '\n';
        });
    if (!subscribed.has_value()) {
        return 1;
    }
    if (!system.start(*feedback_actor).has_value()) {
        return 1;
    }
    const xstate::event update{
        .type = "feedback.update",
        .payload = {{"feedback", "Some other feedback"}},
    };
    if (!system.send(*feedback_actor, update).has_value()) {
        return 1;
    }
    // end::actor[]
    return 0;
}
