// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

/** The types of `actions`, as a list: [a, b]. */
std::string types_of(const std::vector<xstate::action>& actions) {
    std::string listed = "[";
    for (const xstate::action& one : actions) {
        if (listed.size() > 1) {
            listed += ", ";
        }
        listed += one.type;
    }
    return listed + "]";
}

/** Whether a call that delivers ran to its end: no error, and no actor left parked. */
bool settled(const xstate::result<xstate::run_outcome>& ran) {
    return ran.has_value() && *ran == xstate::run_outcome::settled;
}

}  // namespace

int main() {
    // tag::machine[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "dog",
        "initial": "asleep",
        "states": {
            "asleep": {"on": {"wakes up": "stretching"}},
            "stretching": {"always": "awake"},
            "awake": {"entry": "bark"}
        }
    })");
    const xstate::result<xstate::machine> dog = xstate::create_machine(config, {});
    if (!dog.has_value()) {
        return 1;
    }
    const xstate::event wakes_up{.type = "wakes up", .payload = {}};
    // end::machine[]

    // tag::core[]
    const xstate::snapshot asleep = xstate::get_initial_snapshot(*dog);
    xstate::macrostep step = xstate::begin(*dog, asleep, wakes_up);
    while (!step.done()) {
        const xstate::progress made = step.next();
        std::cout << "microstep: " << boost::json::serialize(made.step.snapshot.value) << ' '
                  << types_of(made.step.actions) << '\n';
    }
    std::cout << "settled: " << boost::json::serialize(step.result().snapshot.value) << '\n';
    // end::core[]

    // tag::actor[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<void> acting =
        system.on_action([](xstate::actor_ref, const xstate::action& custom) {
            std::cout << "custom action: " << custom.type << '\n';
        });
    if (!acting.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> dog_actor = system.create_actor(*dog);
    if (!dog_actor.has_value() || !settled(system.start(*dog_actor)) ||
        !settled(system.send(*dog_actor, wakes_up))) {
        return 1;
    }
    const xstate::snapshot* now = system.snapshot_of(*dog_actor);
    if (now == nullptr) {
        return 1;
    }
    std::cout << "actor: " << boost::json::serialize(now->value) << '\n';
    // end::actor[]
    return 0;
}
