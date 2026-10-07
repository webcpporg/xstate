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
/** Prints the actor's state value, its context and the ids of its children. */
void print(const xstate::actor_system& system, xstate::actor_ref actor) {
    const xstate::snapshot* snapshot = system.snapshot_of(actor);
    if (snapshot == nullptr) {
        return;
    }
    std::cout << "app: " << boost::json::serialize(snapshot->value) << ' '
              << boost::json::serialize(snapshot->context) << " children [";
    std::string_view separator;
    for (const auto& [id, src] : snapshot->children) {
        std::cout << separator << id;
        separator = ", ";
    }
    std::cout << "]\n";
}

// end::print[]

}  // namespace

int main() {
    std::cout << std::boolalpha;
    // tag::actors[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "app",
        "initial": "loading",
        "context": {"user": null},
        "states": {
            "loading": {
                "invoke": {
                    "id": "fetch",
                    "src": "fetchUser",
                    "systemId": "fetcher",
                    "input": {"userId": 42},
                    "onDone": {"target": "ready", "actions": "keepUser"}
                }
            },
            "ready": {}
        }
    })");

    const xstate::assigner keep_user =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::value* output = args.event.payload.if_contains("output");
        if (output == nullptr) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"user", *output}};
    };

    xstate::implementations implementations;
    implementations.actors.emplace("fetchUser", xstate::host_actor{});
    implementations.actions.emplace("keepUser", xstate::assign_action{.assignment = keep_user});

    const xstate::result<xstate::machine> app = xstate::create_machine(config, implementations);
    if (!app.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*app);
    if (!actor.has_value() || !system.start(*actor).has_value()) {
        return 1;
    }
    print(system, *actor);
    for (const xstate::host_request& request : system.host_requests()) {
        std::cout << "request: " << request.id << ' ' << request.src << ' '
                  << boost::json::serialize(request.input.value_or(nullptr)) << '\n';
        std::cout << "fetcher is the request's actor: " << (system.get("fetcher") == request.actor)
                  << '\n';
        if (!system.resolve(request, boost::json::object{{"name", "Ana"}}).has_value()) {
            return 1;
        }
    }
    print(system, *actor);
    std::cout << "fetcher is registered: " << system.get("fetcher").has_value() << '\n';
    // end::actors[]
    return 0;
}
