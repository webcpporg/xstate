// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    const xstate::assigner count_user =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* users =
            context == nullptr ? nullptr : context->if_contains("users");
        if (users == nullptr || !users->is_int64()) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"users", users->get_int64() + 1}};
    };
    const xstate::event_maker started =
        [](const xstate::action_args&) -> xstate::result<xstate::event> {
        return xstate::event{.type = "STARTED", .payload = {}};
    };
    xstate::implementations implementations;
    implementations.actions.emplace("countUser", xstate::assign_action{.assignment = count_user});
    implementations.actions.emplace(
        "announce",
        xstate::raise_action{.event = started, .id = std::nullopt, .delay = std::nullopt});

    const boost::json::value config = boost::json::parse(R"({
        "initial": "idle",
        "context": {"users": 0},
        "states": {
            "idle": {
                "on": {
                    "processUser": {
                        "target": "processing",
                        "actions": [
                            {"type": "sendEmail", "params": {"subject": "Processing started"}},
                            "countUser",
                            "announce"
                        ]
                    }
                }
            },
            "processing": {"on": {"STARTED": "started"}},
            "started": {}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, implementations);
    if (!machine.has_value()) {
        return 1;
    }

    const xstate::snapshot idle = xstate::get_initial_snapshot(*machine);
    const xstate::event process_user{
        .type = "processUser",
        .payload = {{"userId", "123"}, {"email", "user@example.com"}},
    };
    const auto [next_state, actions] = xstate::transition(*machine, idle, process_user);
    for (const xstate::action& action : actions) {
        std::cout << action.type << ' ' << boost::json::serialize(action.params) << '\n';
    }
    std::cout << "value: " << boost::json::serialize(next_state.value) << '\n';
    std::cout << "context: " << boost::json::serialize(next_state.context) << '\n';
    // end::example[]
    return 0;
}
