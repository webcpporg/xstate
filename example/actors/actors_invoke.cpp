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

xstate::result<std::optional<boost::json::value>> the_event(const xstate::action_args& args) {
    return std::optional<boost::json::value>(xstate::to_json(args.event));
}

/** Prints an actor's state value and the ids of its children. */
void print(const xstate::actor_system& system, xstate::actor_ref actor) {
    const xstate::snapshot* current = system.snapshot_of(actor);
    if (current == nullptr) {
        return;
    }
    boost::json::array children;
    for (const auto& [id, src] : current->children) {
        children.emplace_back(id);
    }
    std::cout << boost::json::serialize(current->value) << ' ' << boost::json::serialize(children)
              << '\n';
}

}  // namespace

int main() {
    // tag::machines[]
    const boost::json::value worker_config = boost::json::parse(R"({
        "initial": "working",
        "states": {
            "working": {"on": {"finish": "done"}},
            "done": {"type": "final"}
        },
        "output": {"result": 42}
    })");
    const xstate::result<xstate::machine> worker = xstate::create_machine(worker_config, {});
    if (!worker.has_value()) {
        return 1;
    }

    xstate::implementations implementations;
    implementations.actors.emplace("worker", xstate::machine_actor{*worker});
    implementations.actions.emplace("note",
                                    xstate::log_action{.value = the_event, .label = "done"});
    const boost::json::value parent_config = boost::json::parse(R"({
        "initial": "waiting",
        "states": {
            "waiting": {
                "invoke": {
                    "src": "worker",
                    "id": "worker",
                    "onDone": {"target": "finished", "actions": "note"}
                }
            },
            "finished": {}
        }
    })");
    const xstate::result<xstate::machine> parent_machine =
        xstate::create_machine(parent_config, implementations);
    if (!parent_machine.has_value()) {
        return 1;
    }
    // end::machines[]

    // tag::run[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<void> logging = system.on_log(
        [](xstate::actor_ref, const boost::json::value* value, std::string_view label) {
            if (value != nullptr) {
                std::cout << label << ' ' << boost::json::serialize(*value) << '\n';
            }
        });
    if (!logging.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> parent = system.create_actor(*parent_machine);
    if (!parent.has_value()) {
        return 1;
    }
    if (!system.start(*parent).has_value()) {
        return 1;
    }
    print(system, *parent);  // "waiting" ["worker"]

    const std::optional<xstate::actor_ref> child = system.child_of(*parent, "worker");
    if (!child.has_value()) {
        return 1;
    }
    if (!system.send(*child, {.type = "finish", .payload = {}}).has_value()) {
        return 1;
    }
    // logs the done event onDone took
    print(system, *parent);  // "finished" []

    const xstate::result<xactor::status> status = system.status_of(*child);
    if (!status.has_value()) {
        return 1;
    }
    std::cout << "worker: " << name_of(*status) << '\n';
    // end::run[]
    return 0;
}
