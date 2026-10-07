// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

// tag::implementations[]
xstate::result<xstate::event> pong(const xstate::action_args& /*args*/) {
    return xstate::event{.type = "pong", .payload = {}};
}

xstate::result<std::optional<boost::json::value>> the_event(const xstate::action_args& args) {
    return std::optional<boost::json::value>(xstate::to_json(args.event));
}

// end::implementations[]

}  // namespace

int main() {
    // tag::watching[]
    xstate::implementations implementations;
    implementations.actions.emplace("answer", xstate::emit_action{.event = pong});
    implementations.actions.emplace("note",
                                    xstate::log_action{.value = the_event, .label = "received"});
    // greet has no implementation: it is a custom action, the host's to run.
    const boost::json::value config = boost::json::parse(R"({
        "entry": {"type": "greet", "params": {"name": "Ada"}},
        "on": {"ping": {"actions": ["answer", "note"]}}
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, implementations);
    if (!machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<void> acting =
        system.on_action([](xstate::actor_ref, const xstate::action& custom) {
            std::cout << "action " << custom.type << ' ' << boost::json::serialize(custom.params)
                      << '\n';
        });
    if (!acting.has_value()) {
        return 1;
    }
    const xstate::result<void> logging = system.on_log(
        [](xstate::actor_ref, const boost::json::value* value, std::string_view label) {
            if (value != nullptr) {
                std::cout << "log " << label << ' ' << boost::json::serialize(*value) << '\n';
            }
        });
    if (!logging.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*machine);
    if (!actor.has_value()) {
        return 1;
    }
    const xstate::result<void> listening =
        system.on_emitted(*actor, [](const xstate::event& emitted) {
            std::cout << "emitted " << boost::json::serialize(xstate::to_json(emitted)) << '\n';
        });
    if (!listening.has_value()) {
        return 1;
    }

    if (!system.start(*actor).has_value()) {
        return 1;
    }
    // logs: action greet {"name":"Ada"}
    if (!system.send(*actor, {.type = "ping", .payload = {}}).has_value()) {
        return 1;
    }
    // logs: log received {"type":"ping"}
    // logs: emitted {"type":"pong"}
    // end::watching[]
    return 0;
}
