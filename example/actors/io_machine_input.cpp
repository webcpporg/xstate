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

// tag::context[]
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

// end::context[]

}  // namespace

int main() {
    // tag::machine[]
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

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> feedback_actor = system.create_actor(
        *feedback_machine, {.input = boost::json::object{{"userId", "123"}, {"defaultRating", 5}}});
    if (!feedback_actor.has_value() || !system.start(*feedback_actor).has_value()) {
        return 1;
    }
    const xstate::snapshot* started = system.snapshot_of(*feedback_actor);
    if (started == nullptr) {
        return 1;
    }
    std::cout << boost::json::serialize(started->context) << '\n';
    // end::machine[]
    return 0;
}
