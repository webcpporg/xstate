// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string>
#include <string_view>

namespace xstate = webcpp::xstate;
namespace xactor = webcpp::xactor;

namespace {

std::string_view status_name(xactor::status of) {
    switch (of) {
        case xactor::status::active: return "active";
        case xactor::status::done: return "done";
        case xactor::status::error: return "error";
        case xactor::status::stopped: return "stopped";
    }
    return "unknown";
}

// tag::settled[]
/** Whether a call that delivers ran to its end: no error, and no actor left parked. */
bool settled(const xstate::result<xstate::run_outcome>& ran) {
    return ran.has_value() && *ran == xstate::run_outcome::settled;
}

// end::settled[]

}  // namespace

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "dog",
        "initial": "asleep",
        "states": {
            "asleep": {"on": {"wakes up": "awake"}},
            "awake": {"on": {"falls asleep": "asleep"}}
        }
    })");
    const xstate::result<xstate::machine> dog = xstate::create_machine(config, {});
    if (!dog.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> dog_actor = system.create_actor(*dog);
    if (!dog_actor.has_value()) {
        return 1;
    }
    const xstate::result<void> subscribed =
        system.subscribe(*dog_actor, [](const xstate::snapshot& snapshot) {
            std::cout << "snapshot: " << boost::json::serialize(snapshot.value) << '\n';
        });
    if (!subscribed.has_value()) {
        return 1;
    }
    if (!settled(system.start(*dog_actor))) {
        return 1;
    }
    for (const std::string_view type : {"wakes up", "falls asleep"}) {
        if (!settled(system.send(*dog_actor, {.type = std::string(type), .payload = {}}))) {
            return 1;
        }
    }
    if (!system.stop(*dog_actor).has_value()) {
        return 1;
    }
    const xstate::result<xactor::status> ended = system.status_of(*dog_actor);
    if (!ended.has_value()) {
        return 1;
    }
    std::cout << "status: " << status_name(*ended) << '\n';
    // end::example[]
    return 0;
}
