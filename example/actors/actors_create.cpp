// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string_view>

namespace xstate = webcpp::xstate;
namespace xactor = webcpp::xactor;

namespace {

std::string_view name_of(xactor::status status) {
    switch (status) {
        case xactor::status::active: return "active";
        case xactor::status::done: return "done";
        case xactor::status::error: return "error";
        case xactor::status::stopped: return "stopped";
    }
    return "unknown";
}

}  // namespace

int main() {
    // tag::create[]
    const boost::json::value config = boost::json::parse(R"({
        "initial": "inactive",
        "states": {
            "inactive": {"on": {"toggle": {"target": "active"}}},
            "active": {"on": {"toggle": {"target": "inactive"}}}
        }
    })");
    const xstate::result<xstate::machine> toggle_machine = xstate::create_machine(config, {});
    if (!toggle_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> toggle_actor = system.create_actor(*toggle_machine);
    if (!toggle_actor.has_value()) {
        return 1;
    }
    const xstate::result<void> subscribed =
        system.subscribe(*toggle_actor, [](const xstate::snapshot& snapshot) {
            std::cout << boost::json::serialize(snapshot.value) << '\n';
        });
    if (!subscribed.has_value()) {
        return 1;
    }

    if (!system.start(*toggle_actor).has_value()) {
        return 1;
    }
    // logs "inactive"
    const xstate::event toggle{.type = "toggle", .payload = {}};
    if (!system.send(*toggle_actor, toggle).has_value()) {
        return 1;
    }
    // logs "active"
    if (!system.send(*toggle_actor, toggle).has_value()) {
        return 1;
    }
    // logs "inactive"

    if (!system.stop(*toggle_actor).has_value()) {
        return 1;
    }
    const xstate::result<xactor::status> status = system.status_of(*toggle_actor);
    if (!status.has_value()) {
        return 1;
    }
    std::cout << "status: " << name_of(*status) << '\n';
    // end::create[]
    return 0;
}
