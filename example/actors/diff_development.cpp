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
#include <string>
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

/** The name XState gives a snapshot's status. */
std::string_view status_name(xstate::status status) {
    switch (status) {
        case xstate::status::active: return "active";
        case xstate::status::done: return "done";
        case xstate::status::error: return "error";
    }
    return "unknown";
}

}  // namespace

int main() {
    // tag::cond[]
    const boost::json::value with_cond = boost::json::parse(R"({
        "id": "m",
        "initial": "a",
        "states": {
            "a": {"on": {"GO": {"target": "b", "cond": "ready"}}},
            "b": {}
        }
    })");
    const xstate::result<xstate::machine> refused = xstate::create_machine(with_cond, {});
    std::cout << "a transition that declares cond: "
              << (refused.has_value() ? "created" : refused.error().message()) << '\n';
    // end::cond[]

    // tag::wildcard[]
    const boost::json::value listener = boost::json::parse(R"({
        "id": "m",
        "on": {"*": {"actions": "heard"}}
    })");
    const xstate::result<xstate::machine> anything = xstate::create_machine(listener, {});
    if (!anything.has_value()) {
        return 1;
    }
    const xstate::snapshot after_star =
        xstate::get_next_snapshot(*anything, xstate::get_initial_snapshot(*anything),
                                  xstate::event{.type = "*", .payload = {}});
    std::cout << "an event of the type * from outside: " << status_name(after_star.status) << ", "
              << after_star.error.message() << '\n';
    // end::wildcard[]

    // tag::forward[]
    xstate::implementations implementations;
    implementations.actions.emplace("relay", xstate::forward_to_action{
                                                 .target = "#system:nobody",
                                                 .id = std::nullopt,
                                                 .delay = std::nullopt,
                                             });
    const boost::json::value relay_config = boost::json::parse(R"({
        "id": "relay",
        "on": {"PING": {"actions": "relay"}}
    })");
    const xstate::result<xstate::machine> relay =
        xstate::create_machine(relay_config, implementations);
    if (!relay.has_value()) {
        return 1;
    }
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*relay);
    if (!actor.has_value()) {
        return 1;
    }
    if (!system.start(*actor).has_value()) {
        return 1;
    }
    if (!system.send(*actor, xstate::event{.type = "PING", .payload = {}}).has_value()) {
        return 1;
    }
    const xstate::snapshot* forwarded = system.snapshot_of(*actor);
    if (forwarded == nullptr) {
        return 1;
    }
    std::cout << "a forwardTo to no actor: " << status_name(forwarded->status) << ", "
              << forwarded->error.message() << '\n';
    // end::forward[]

    // tag::descriptors[]
    const boost::json::value pointer_config = boost::json::parse(R"({
        "id": "pointer",
        "initial": "idle",
        "states": {
            "idle": {"on": {"mouse.*.*": "deep", "mou*se.*": "starred"}},
            "deep": {},
            "starred": {}
        }
    })");
    const xstate::result<xstate::machine> pointer = xstate::create_machine(pointer_config, {});
    if (!pointer.has_value()) {
        return 1;
    }
    const xstate::snapshot idle = xstate::get_initial_snapshot(*pointer);
    for (const std::string_view type : {"mouse.x", "mouse.x", "mou*se.x", "mouse.*.*"}) {
        const xstate::snapshot next = xstate::get_next_snapshot(
            *pointer, idle, xstate::event{.type = std::string(type), .payload = {}});
        std::cout << "the event " << type << ": " << boost::json::serialize(next.value) << '\n';
    }
    // end::descriptors[]
    return 0;
}
