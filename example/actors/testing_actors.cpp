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

// tag::assign[]
/** Counts one more activation in the context. */
xstate::result<boost::json::object> count_activation(const xstate::action_args& args) {
    const boost::json::value* count =
        args.context.is_object() ? args.context.get_object().if_contains("count") : nullptr;
    if (count == nullptr || !count->is_int64()) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"count", count->get_int64() + 1}};
}

// end::assign[]

// tag::expect[]
/** Whether the actor's snapshot has `value` and `context`; prints both. */
bool expect(const xstate::actor_system& system, xstate::actor_ref actor, std::string_view value,
            std::string_view context) {
    const xstate::snapshot* now = system.snapshot_of(actor);
    if (now == nullptr) {
        return false;
    }
    std::cout << boost::json::serialize(now->value) << ' ' << boost::json::serialize(now->context)
              << '\n';
    return now->value == boost::json::parse(value) && now->context == boost::json::parse(context);
}

// end::expect[]

}  // namespace

// tag::main[]
int main() {
    xstate::implementations implementations;
    implementations.actions.emplace("countActivation",
                                    xstate::assign_action{.assignment = count_activation});
    const boost::json::value config = boost::json::parse(R"({
        "initial": "inactive",
        "context": {"count": 0},
        "states": {
            "inactive": {
                "on": {"activate": {"target": "active", "actions": "countActivation"}}
            },
            "active": {"on": {"deactivate": "inactive"}}
        }
    })");
    const xstate::result<xstate::machine> toggle_machine =
        xstate::create_machine(config, implementations);
    if (!toggle_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*toggle_machine);
    if (!actor.has_value() || system.start(*actor) != xstate::run_outcome::settled) {
        return 1;
    }

    // The initial state
    if (!expect(system, *actor, R"("inactive")", R"({"count": 0})")) {
        return 1;
    }

    // Send an event and test the transition
    const xstate::event activate{.type = "activate", .payload = {}};
    if (system.send(*actor, activate) != xstate::run_outcome::settled ||
        !expect(system, *actor, R"("active")", R"({"count": 1})")) {
        return 1;
    }

    // Send another event
    const xstate::event deactivate{.type = "deactivate", .payload = {}};
    if (system.send(*actor, deactivate) != xstate::run_outcome::settled ||
        !expect(system, *actor, R"("inactive")", R"({"count": 1})")) {
        return 1;
    }
    return 0;
}

// end::main[]
