// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

// tag::print[]
/** Prints the state value an actor's snapshot holds, after `moment`. */
void print_value(const xstate::actor_system& system, xstate::actor_ref actor,
                 std::string_view moment) {
    const xstate::snapshot* now = system.snapshot_of(actor);
    if (now == nullptr) {
        return;
    }
    std::cout << moment << ": " << boost::json::serialize(now->value) << '\n';
}

// end::print[]

}  // namespace

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "initial": "a",
        "states": {
            "a": {"on": {"NEXT": "b"}},
            "b": {"on": {"NEXT": "c"}},
            "c": {}
        }
    })");
    const xstate::result<xstate::machine> steps = xstate::create_machine(config, {});
    if (!steps.has_value()) {
        return 1;
    }
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*steps);
    if (!actor.has_value()) {
        return 1;
    }
    const xstate::actor_ref stepper = *actor;
    bool heard_b = false;
    const xstate::result<void> subscribed =
        system.subscribe(stepper, [&system, &heard_b, stepper](const xstate::snapshot& heard) {
            std::cout << "heard " << boost::json::serialize(heard.value) << '\n';
            if (!heard.matches("b")) {
                return;
            }
            heard_b = true;
            const xstate::result<xstate::run_outcome> sent =
                system.send(stepper, {.type = "NEXT", .payload = {}});
            if (!sent.has_value()) {
                const boost::system::error_code& refused = sent.error();
                std::cout << "  the listener's send: " << refused.message() << ", "
                          << refused.category().name() << '\n';
            }
        });
    if (!subscribed.has_value()) {
        return 1;
    }
    if (!system.start(stepper).has_value()) {
        return 1;
    }
    if (!system.send(stepper, {.type = "NEXT", .payload = {}}).has_value()) {
        return 1;
    }
    print_value(system, stepper, "after the host's send");

    // The host acts on what the listener recorded once its own call has returned.
    if (heard_b && !system.send(stepper, {.type = "NEXT", .payload = {}}).has_value()) {
        return 1;
    }
    print_value(system, stepper, "after the host's second send");
    // end::example[]
    return 0;
}
