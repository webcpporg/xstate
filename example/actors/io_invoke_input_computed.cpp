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
    // tag::invoke[]
    // assign({ userId: ({ event }) => event.userId })
    const xstate::assigner select_user =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::value* user_id = args.event.payload.if_contains("userId");
        if (user_id == nullptr) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"userId", *user_id}};
    };
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

    xstate::implementations implementations;
    implementations.actors.emplace("fetchUser", xstate::host_actor{});
    implementations.actions.emplace("selectUser", xstate::assign_action{.assignment = select_user});
    // The input of the invoke whose id is fetchUser.
    implementations.inputs.emplace("fetchUser", user_of_context);
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "context": {"userId": ""},
        "initial": "idle",
        "states": {
            "idle": {"on": {"user.selected": "loading"}},
            "loading": {
                "entry": "selectUser",
                "invoke": {"id": "fetchUser", "src": "fetchUser"}
            }
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
    const xstate::event selected{.type = "user.selected", .payload = {{"userId", "42"}}};
    if (!system.send(*feedback_actor, selected).has_value()) {
        return 1;
    }
    for (const xstate::host_request& request : system.host_requests()) {
        if (request.input.has_value()) {
            std::cout << boost::json::serialize(*request.input) << '\n';
        }
    }
    // end::invoke[]
    return 0;
}
