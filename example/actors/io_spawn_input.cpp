// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>

namespace xstate = webcpp::xstate;

int main() {
    // tag::spawn[]
    // ({ context }) => ({ userId: context.userId })
    const xstate::value_maker user_of_context =
        [](const xstate::action_args& args) -> xstate::result<std::optional<boost::json::value>> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* user_id =
            context == nullptr ? nullptr : context->if_contains("userId");
        if (user_id == nullptr) {
            return xstate::failure<std::optional<boost::json::value>>(
                xstate::errc::implementation_failed);
        }
        return boost::json::value(boost::json::object{{"userId", *user_id}});
    };

    // spawnChild('emailUser', { id: 'emailUser', input: ... })
    const xstate::spawn_child_action spawn_email_user{
        .src = "emailUser",
        .id = "emailUser",
        .system_id = std::nullopt,
        .input = user_of_context,
    };

    xstate::implementations implementations;
    implementations.actors.emplace("emailUser", xstate::host_actor{});
    implementations.actions.emplace("spawnEmailUser", spawn_email_user);
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "context": {"userId": "123"},
        "on": {
            "feedback.submit": {"actions": "spawnEmailUser"}
        }
    })");
    const xstate::result<xstate::machine> feedback_machine =
        xstate::create_machine(config, implementations);
    if (!feedback_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> feedback_actor = system.create_actor(*feedback_machine);
    if (!feedback_actor.has_value() || !system.start(*feedback_actor).has_value()) {
        return 1;
    }
    if (!system.send(*feedback_actor, {.type = "feedback.submit", .payload = {}}).has_value()) {
        return 1;
    }
    for (const xstate::host_request& request : system.host_requests()) {
        if (request.input.has_value()) {
            std::cout << boost::json::serialize(*request.input) << '\n';
        }
    }
    // end::spawn[]
    return 0;
}
