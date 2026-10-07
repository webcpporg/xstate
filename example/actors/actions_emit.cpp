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
    // tag::machine[]
    const xstate::event_maker static_event =
        [](const xstate::action_args&) -> xstate::result<xstate::event> {
        return xstate::event{.type = "someStaticEvent", .payload = {{"data", 42}}};
    };
    const xstate::event_maker dynamic_event =
        [](const xstate::action_args& args) -> xstate::result<xstate::event> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* data =
            context == nullptr ? nullptr : context->if_contains("someData");
        if (data == nullptr) {
            return xstate::failure<xstate::event>(xstate::errc::implementation_failed);
        }
        return xstate::event{.type = "someDynamicEvent", .payload = {{"data", *data}}};
    };
    xstate::implementations implementations;
    implementations.actions.emplace("emitStaticEvent", xstate::emit_action{.event = static_event});
    implementations.actions.emplace("emitDynamicEvent",
                                    xstate::emit_action{.event = dynamic_event});

    const boost::json::value config = boost::json::parse(R"({
        "context": {"someData": "hello"},
        "on": {
            "someEvent": {"actions": ["emitStaticEvent", "emitDynamicEvent", "track"]}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, implementations);
    if (!machine.has_value()) {
        return 1;
    }
    // end::machine[]

    // tag::listen[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*machine);
    if (!actor.has_value()) {
        return 1;
    }
    const xstate::result<void> hearing_static =
        system.on_emitted(*actor, [](const xstate::event& emitted) {
            if (emitted.type == "someStaticEvent") {
                std::cout << "static: " << boost::json::serialize(xstate::to_json(emitted)) << '\n';
            }
        });
    if (!hearing_static.has_value()) {
        return 1;
    }
    const xstate::result<void> hearing_any = system.on_emitted(
        *actor, [](const xstate::event& emitted) { std::cout << "any: " << emitted.type << '\n'; });
    if (!hearing_any.has_value()) {
        return 1;
    }
    const xstate::result<void> acting =
        system.on_action([](xstate::actor_ref, const xstate::action& custom) {
            std::cout << "action: " << custom.type << '\n';
        });
    if (!acting.has_value()) {
        return 1;
    }

    if (!system.start(*actor).has_value() ||
        !system.send(*actor, {.type = "someEvent", .payload = {}}).has_value()) {
        return 1;
    }
    // end::listen[]
    return 0;
}
