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

std::string_view status_name(xstate::status of) {
    switch (of) {
        case xstate::status::active: return "active";
        case xstate::status::done: return "done";
        case xstate::status::error: return "error";
    }
    return "unknown";
}

/** Prints who an actor is, its state value and its status, or that it has no snapshot. */
void print(std::string_view who, const xstate::snapshot* of) {
    if (of == nullptr) {
        std::cout << who << ": no snapshot\n";
        return;
    }
    std::cout << who << ": " << boost::json::serialize(of->value) << ", "
              << status_name(of->status);
    if (of->output.has_value()) {
        std::cout << ", output " << boost::json::serialize(*of->output);
    }
    std::cout << '\n';
}

/** Whether a call that delivers ran to its end: no error, and no actor left parked. */
bool settled(const xstate::result<xstate::run_outcome>& ran) {
    return ran.has_value() && *ran == xstate::run_outcome::settled;
}

}  // namespace

int main() {
    // tag::example[]
    const boost::json::value dog_config = boost::json::parse(R"({
        "id": "dog",
        "initial": "sniffing",
        "states": {
            "sniffing": {"on": {"finds a stick": "fetched"}},
            "fetched": {"type": "final"}
        },
        "output": {"brought": "a stick"}
    })");
    const xstate::result<xstate::machine> dog = xstate::create_machine(dog_config, {});
    if (!dog.has_value()) {
        return 1;
    }

    xstate::implementations implementations;
    implementations.actors.emplace("dog", xstate::machine_actor{*dog});
    const boost::json::value owner_config = boost::json::parse(R"({
        "id": "owner",
        "initial": "walking",
        "states": {
            "walking": {"invoke": {"id": "dog", "src": "dog", "onDone": "home"}},
            "home": {"type": "final"}
        }
    })");
    const xstate::result<xstate::machine> owner =
        xstate::create_machine(owner_config, implementations);
    if (!owner.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> owner_actor = system.create_actor(*owner);
    if (!owner_actor.has_value() || !settled(system.start(*owner_actor))) {
        return 1;
    }
    const std::optional<xstate::actor_ref> dog_actor = system.child_of(*owner_actor, "dog");
    if (!dog_actor.has_value()) {
        return 1;
    }
    print("owner", system.snapshot_of(*owner_actor));
    print("dog", system.snapshot_of(*dog_actor));

    if (!settled(system.send(*dog_actor, {.type = "finds a stick", .payload = {}}))) {
        return 1;
    }
    print("owner", system.snapshot_of(*owner_actor));
    print("dog", system.snapshot_of(*dog_actor));
    // end::example[]
    return 0;
}
