// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    // tag::assign[]
    const xstate::assigner add_value =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* count =
            context == nullptr ? nullptr : context->if_contains("count");
        const boost::json::value* value = args.event.payload.if_contains("value");
        if (count == nullptr || !count->is_int64() || value == nullptr || !value->is_int64()) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"count", count->get_int64() + value->get_int64()}};
    };
    xstate::implementations implementations;
    implementations.actions.emplace("addValue", xstate::assign_action{.assignment = add_value});

    const boost::json::value config = boost::json::parse(R"({
        "context": {"count": 0, "unit": "clicks"},
        "on": {"increment": {"actions": "addValue"}}
    })");
    const xstate::result<xstate::machine> count_machine =
        xstate::create_machine(config, implementations);
    if (!count_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> count_actor = system.create_actor(*count_machine);
    if (!count_actor.has_value()) {
        return 1;
    }
    const xstate::result<void> subscribed =
        system.subscribe(*count_actor, [](const xstate::snapshot& state) {
            std::cout << boost::json::serialize(state.context) << '\n';
        });
    if (!subscribed.has_value()) {
        return 1;
    }
    if (!system.start(*count_actor).has_value()) {
        return 1;
    }
    if (!system.send(*count_actor, {.type = "increment", .payload = {{"value", 3}}}).has_value()) {
        return 1;
    }
    if (!system.send(*count_actor, {.type = "increment", .payload = {{"value", 2}}}).has_value()) {
        return 1;
    }
    // end::assign[]
    return 0;
}
