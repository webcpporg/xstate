// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>
#include <string>
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

/** Prints the ids of an actor's children. */
void print_children(const xstate::actor_system& system, xstate::actor_ref actor) {
    const xstate::snapshot* current = system.snapshot_of(actor);
    if (current == nullptr) {
        return;
    }
    boost::json::array children;
    for (const auto& [id, src] : current->children) {
        children.emplace_back(id);
    }
    std::cout << boost::json::serialize(children) << '\n';
}

// tag::spawning[]
/** XState's spawnChild('childMachine', { id }). */
xstate::spawn_child_action spawning(std::string_view id) {
    return xstate::spawn_child_action{
        .src = "childMachine",
        .id = std::string(id),
        .system_id = std::nullopt,
        .input = std::nullopt,
    };
}

// end::spawning[]

}  // namespace

int main() {
    // tag::machine[]
    const boost::json::value child_config = boost::json::parse(R"({"id": "child"})");
    const xstate::result<xstate::machine> child_machine = xstate::create_machine(child_config, {});
    if (!child_machine.has_value()) {
        return 1;
    }

    xstate::implementations implementations;
    implementations.actors.emplace("childMachine", xstate::machine_actor{*child_machine});
    implementations.actions.emplace("spawnChild1", spawning("child-1"));
    implementations.actions.emplace("spawnChild2", spawning("child-2"));
    implementations.actions.emplace("spawnChild3", spawning("child-3"));
    implementations.actions.emplace("stopChild2", xstate::stop_child_action{.id = "child-2"});
    const boost::json::value config = boost::json::parse(R"({
        "entry": ["spawnChild1", "spawnChild2", "spawnChild3"],
        "on": {"STOP": {"actions": "stopChild2"}}
    })");
    const xstate::result<xstate::machine> parent_machine =
        xstate::create_machine(config, implementations);
    if (!parent_machine.has_value()) {
        return 1;
    }
    // end::machine[]

    // tag::run[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> parent = system.create_actor(*parent_machine);
    if (!parent.has_value()) {
        return 1;
    }
    if (!system.start(*parent).has_value()) {
        return 1;
    }
    print_children(system, *parent);  // ["child-1","child-2","child-3"]

    const std::optional<xstate::actor_ref> child2 = system.child_of(*parent, "child-2");
    if (!child2.has_value()) {
        return 1;
    }
    if (!system.send(*parent, {.type = "STOP", .payload = {}}).has_value()) {
        return 1;
    }
    print_children(system, *parent);  // ["child-1","child-3"]

    const xstate::result<xactor::status> status = system.status_of(*child2);
    if (!status.has_value()) {
        return 1;
    }
    std::cout << "child-2: " << name_of(*status) << '\n';
    // end::run[]
    return 0;
}
