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
    // tag::raise[]
    const xstate::event_maker started =
        [](const xstate::action_args& args) -> xstate::result<xstate::event> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* attempt =
            context == nullptr ? nullptr : context->if_contains("attempt");
        if (attempt == nullptr) {
            return xstate::failure<xstate::event>(xstate::errc::implementation_failed);
        }
        return xstate::event{.type = "started", .payload = {{"attempt", *attempt}}};
    };
    xstate::implementations implementations;
    implementations.actions.emplace(
        "announce",
        xstate::raise_action{.event = started, .id = std::nullopt, .delay = std::nullopt});

    const boost::json::value config = boost::json::parse(R"({
        "id": "loader",
        "initial": "idle",
        "context": {"attempt": 1},
        "states": {
            "idle": {"on": {"load": "loading"}},
            "loading": {"entry": "announce", "on": {"started": "running"}},
            "running": {}
        }
    })");
    const xstate::result<xstate::machine> loader = xstate::create_machine(config, implementations);
    if (!loader.has_value()) {
        return 1;
    }

    const xstate::snapshot idle = xstate::get_initial_snapshot(*loader);
    for (const xstate::microstep& step :
         xstate::get_microsteps(*loader, idle, {.type = "load", .payload = {}})) {
        std::cout << boost::json::serialize(step.snapshot.value);
        for (const xstate::action& one : step.actions) {
            std::cout << ' ' << one.type << ' ' << boost::json::serialize(one.params);
        }
        std::cout << '\n';
    }
    // end::raise[]
    return 0;
}
