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
#include <string_view>

namespace xstate = webcpp::xstate;

int main() {
    // tag::init[]
    xstate::implementations implementations;
    // XState's ({ event }) => console.log(event), as a log of the event.
    const xstate::value_maker the_event =
        [](const xstate::action_args& args) -> xstate::result<std::optional<boost::json::value>> {
        return boost::json::value(xstate::to_json(args.event));
    };
    implementations.actions.emplace("logEvent",
                                    xstate::log_action{.value = the_event, .label = std::nullopt});
    const boost::json::value config = boost::json::parse(R"({"entry": "logEvent"})");
    const xstate::result<xstate::machine> feedback_machine =
        xstate::create_machine(config, implementations);
    if (!feedback_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<void> logging =
        system.on_log([](xstate::actor_ref, const boost::json::value* value, std::string_view) {
            if (value != nullptr) {
                std::cout << boost::json::serialize(*value) << '\n';
            }
        });
    if (!logging.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> feedback_actor = system.create_actor(
        *feedback_machine, {.input = boost::json::object{{"userId", "123"}, {"defaultRating", 5}}});
    if (!feedback_actor.has_value() || !system.start(*feedback_actor).has_value()) {
        return 1;
    }
    // end::init[]
    return 0;
}
