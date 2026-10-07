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

/** XState's spawnChild('childMachine', { systemId }): no id. */
xstate::spawn_child_action spawning(std::string_view system_id) {
    return xstate::spawn_child_action{
        .src = "childMachine",
        .id = std::string(),
        .system_id = std::string(system_id),
        .input = std::nullopt,
    };
}

}  // namespace

int main() {
    // tag::example[]
    const boost::json::value child_config = boost::json::parse(R"({"id": "child"})");
    const xstate::result<xstate::machine> child_machine = xstate::create_machine(child_config, {});
    if (!child_machine.has_value()) {
        return 1;
    }
    xstate::implementations implementations;
    implementations.actors.emplace("childMachine", xstate::machine_actor{*child_machine});
    implementations.actions.emplace("spawnFirst", spawning("first"));
    implementations.actions.emplace("spawnSecond", spawning("second"));
    const boost::json::value config = boost::json::parse(R"({
        "entry": ["spawnFirst", "spawnSecond"]
    })");
    const xstate::result<xstate::machine> parent_machine =
        xstate::create_machine(config, implementations);
    if (!parent_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> parent = system.create_actor(*parent_machine);
    if (!parent.has_value() || !system.start(*parent).has_value()) {
        return 1;
    }
    const xstate::snapshot* current = system.snapshot_of(*parent);
    if (current == nullptr) {
        return 1;
    }
    boost::json::array keys;
    for (const auto& [id, src] : current->children) {
        keys.emplace_back(id);
    }
    std::cout << boost::json::serialize(keys) << '\n';
    const std::optional<xstate::actor_ref> keyed = system.child_of(*parent, "");
    for (const std::string_view name : {"first", "second"}) {
        const std::optional<xstate::actor_ref> child = system.get(name);
        if (!child.has_value()) {
            return 1;
        }
        const xstate::result<xactor::status> status = system.status_of(*child);
        if (!status.has_value()) {
            return 1;
        }
        std::cout << name << ": " << name_of(*status)
                  << ", keyed: " << (keyed == child ? "true" : "false") << '\n';
    }
    // end::example[]
    return 0;
}
