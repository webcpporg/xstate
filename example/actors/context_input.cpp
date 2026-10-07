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

// tag::context_maker[]
xstate::result<boost::json::value> initial_feedback(const boost::json::value& input) {
    const boost::json::object* given = input.if_object();
    const boost::json::value* rating =
        given == nullptr ? nullptr : given->if_contains("defaultRating");
    if (rating == nullptr || !rating->is_number()) {
        return xstate::failure<boost::json::value>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"feedback", ""}, {"rating", *rating}};
}

// end::context_maker[]
}  // namespace

int main() {
    // tag::input[]
    xstate::implementations implementations;
    implementations.context = initial_feedback;
    const boost::json::value config = boost::json::parse(R"({"id": "feedback"})");
    const xstate::result<xstate::machine> feedback_machine =
        xstate::create_machine(config, implementations);
    if (!feedback_machine.has_value()) {
        return 1;
    }

    const boost::json::value input = boost::json::parse(R"({"defaultRating": 5})");
    const xstate::snapshot snapshot = xstate::get_initial_snapshot(*feedback_machine, input);
    std::cout << boost::json::serialize(snapshot.context) << '\n';
    // end::input[]

    // tag::missing[]
    const xstate::snapshot without = xstate::get_initial_snapshot(*feedback_machine);
    std::cout << (without.status == xstate::status::error ? "error " : "active ")
              << boost::json::serialize(without.context) << '\n';
    // end::missing[]

    // tag::actor[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> feedback_actor =
        system.create_actor(*feedback_machine, {.input = input});
    if (!feedback_actor.has_value()) {
        return 1;
    }
    if (!system.start(*feedback_actor).has_value()) {
        return 1;
    }
    const xstate::snapshot* started = system.snapshot_of(*feedback_actor);
    if (started == nullptr) {
        return 1;
    }
    std::cout << boost::json::serialize(started->context) << '\n';
    // end::actor[]
    return 0;
}
