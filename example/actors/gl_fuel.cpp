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

namespace {

// tag::print[]
/** Prints how a host call ended and the snapshot the actor has published. */
void print(std::string_view call, const xstate::result<xstate::run_outcome>& outcome,
           const xstate::actor_system& system, xstate::actor_ref actor) {
    if (!outcome.has_value()) {
        std::cout << call << ": " << outcome.error().message() << '\n';
        return;
    }
    const xstate::snapshot* published = system.snapshot_of(actor);
    std::cout << call << ": "
              << (*outcome == xstate::run_outcome::settled ? "settled" : "out_of_fuel") << ", "
              << (published == nullptr ? "no snapshot" : boost::json::serialize(published->value))
              << '\n';
}

// end::print[]

}  // namespace

int main() {
    // tag::fuel[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "steps",
        "initial": "idle",
        "states": {
            "idle": {"on": {"go": "one"}},
            "one": {"always": "two"},
            "two": {"always": "three"},
            "three": {}
        }
    })");
    const xstate::result<xstate::machine> steps = xstate::create_machine(config, {});
    if (!steps.has_value()) {
        return 1;
    }

    // Each execution has two units of fuel; start, send and resume each open one, and `go` takes
    // three microsteps.
    xstate::actor_system system({.fuel = 2});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*steps);
    if (!actor.has_value()) {
        return 1;
    }
    print("start", system.start(*actor), system, *actor);
    print("send go", system.send(*actor, xstate::event{.type = "go", .payload = {}}), system,
          *actor);
    print("resume", system.resume(), system, *actor);
    // end::fuel[]
    return 0;
}
