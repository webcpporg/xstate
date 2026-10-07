// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <iostream>
#include <optional>

namespace xstate = webcpp::xstate;

namespace {

xstate::result<xstate::event> some_event(const xstate::action_args& /*args*/) {
    return xstate::event{.type = "someEvent", .payload = {}};
}

/** {count: context.count + 1} */
xstate::result<boost::json::object> increment(const xstate::action_args& args) {
    const boost::json::object* context = args.context.if_object();
    const boost::json::value* count = context == nullptr ? nullptr : context->if_contains("count");
    if (count == nullptr || !count->is_int64()) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"count", count->get_int64() + 1}};
}

/** Prints how many someEvent the child someActor has received. */
void print_count(const xstate::actor_system& system, xstate::actor_ref parent) {
    const std::optional<xstate::actor_ref> child = system.child_of(parent, "someActor");
    if (!child.has_value()) {
        return;
    }
    const xstate::snapshot* current = system.snapshot_of(*child);
    if (current == nullptr) {
        return;
    }
    const boost::json::object* context = current->context.if_object();
    if (context == nullptr) {
        return;
    }
    const boost::json::value* count = context->if_contains("count");
    if (count == nullptr) {
        return;
    }
    std::cout << "count " << boost::json::serialize(*count) << '\n';
}

}  // namespace

int main() {
    // tag::machines[]
    xstate::implementations some_actor_implementations;
    some_actor_implementations.actions.emplace("count",
                                               xstate::assign_action{.assignment = increment});
    const boost::json::value some_actor_config = boost::json::parse(R"({
        "context": {"count": 0},
        "on": {"someEvent": {"actions": "count"}}
    })");
    const xstate::result<xstate::machine> some_actor =
        xstate::create_machine(some_actor_config, some_actor_implementations);
    if (!some_actor.has_value()) {
        return 1;
    }

    xstate::implementations implementations;
    implementations.actors.emplace("someActor", xstate::machine_actor{*some_actor});
    const xstate::send_to_action transmit{
        .target = "someActor",
        .event = some_event,
        .id = "someId",
        .delay = xstate::delay_ref{std::uint64_t{1000}},
    };
    implementations.actions.emplace("transmit", transmit);
    implementations.actions.emplace("cancelIt", xstate::cancel_action{.id = "someId"});
    const boost::json::value config = boost::json::parse(R"({
        "invoke": {"id": "someActor", "src": "someActor"},
        "on": {
            "event": {"actions": "transmit"},
            "cancelEvent": {"actions": "cancelIt"}
        }
    })");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, implementations);
    if (!machine.has_value()) {
        return 1;
    }
    // end::machines[]

    // tag::run[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*machine);
    if (!actor.has_value()) {
        return 1;
    }
    if (!system.start(*actor).has_value()) {
        return 1;
    }
    // At 0 ms: armed for 1000 ms, then cancelled.
    if (!system.send(*actor, {.type = "event", .payload = {}}).has_value()) {
        return 1;
    }
    if (!system.send(*actor, {.type = "cancelEvent", .payload = {}}).has_value()) {
        return 1;
    }
    if (!system.clock_tick(1000).has_value()) {
        return 1;
    }
    print_count(system, *actor);  // count 0

    // At 1000 ms: armed for 2000 ms.
    if (!system.send(*actor, {.type = "event", .payload = {}}).has_value()) {
        return 1;
    }
    if (!system.clock_tick(1999).has_value()) {
        return 1;
    }
    print_count(system, *actor);  // count 0
    if (!system.clock_tick(2000).has_value()) {
        return 1;
    }
    print_count(system, *actor);  // count 1
    // end::run[]
    return 0;
}
