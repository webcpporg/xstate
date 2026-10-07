// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

// tag::tick[]
/** Moves the system's clock to `now`; whether the actor is then in `expected`, which it prints. */
bool tick(xstate::actor_system& system, xstate::actor_ref actor, std::uint64_t now,
          std::string_view expected) {
    if (system.clock_tick(now) != xstate::run_outcome::settled) {
        return false;
    }
    const xstate::snapshot* current = system.snapshot_of(actor);
    if (current == nullptr) {
        return false;
    }
    std::cout << "after " << now << " ms: " << boost::json::serialize(current->value) << '\n';
    return current->value == boost::json::value(expected);
}

// end::tick[]

}  // namespace

// tag::main[]
int main() {
    const boost::json::value config = boost::json::parse(R"({
        "id": "light",
        "initial": "green",
        "states": {
            "green": {"after": {"1000": "yellow"}},
            "yellow": {"after": {"1000": "red"}},
            "red": {"after": {"1000": "green"}}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*machine);
    if (!actor.has_value() || system.start(*actor) != xstate::run_outcome::settled) {
        return 1;
    }

    if (!tick(system, *actor, 500, "green") || !tick(system, *actor, 1010, "yellow")) {
        return 1;
    }
    return 0;
}

// end::main[]
