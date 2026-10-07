// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace xstate = webcpp::xstate;

int main() {
    // tag::machine[]
    const xstate::event_maker nudge =
        [](const xstate::action_args&) -> xstate::result<xstate::event> {
        return xstate::event{.type = "nudge", .payload = {}};
    };
    const xstate::raise_action schedule_nudge{
        .event = nudge,
        .id = "nudge",
        .delay = xstate::delay_ref{std::uint64_t{1000}},
    };
    xstate::implementations implementations;
    implementations.actions.emplace("scheduleNudge", schedule_nudge);
    implementations.actions.emplace("cancelNudge", xstate::cancel_action{.id = "nudge"});

    const boost::json::value config = boost::json::parse(R"({
        "id": "reminder",
        "initial": "waiting",
        "states": {
            "waiting": {
                "on": {
                    "remind": {"actions": "scheduleNudge"},
                    "dismiss": {"actions": "cancelNudge"},
                    "nudge": "nudged"
                }
            },
            "nudged": {}
        }
    })");
    const xstate::result<xstate::machine> reminder =
        xstate::create_machine(config, implementations);
    if (!reminder.has_value()) {
        return 1;
    }
    // end::machine[]

    // tag::returned[]
    const xstate::snapshot waiting = xstate::get_initial_snapshot(*reminder);
    for (const std::string_view type : {"remind", "dismiss"}) {
        const auto [next, actions] =
            xstate::transition(*reminder, waiting, {.type = std::string(type), .payload = {}});
        for (const xstate::action& one : actions) {
            std::cout << type << ": " << one.type << ' ' << boost::json::serialize(one.params)
                      << '\n';
        }
    }
    // end::returned[]

    // tag::timers[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*reminder);
    if (!actor.has_value() || !system.start(*actor).has_value()) {
        return 1;
    }
    const auto print_value = [&system, &actor](std::string_view when) {
        const xstate::snapshot* now = system.snapshot_of(*actor);
        if (now == nullptr) {
            return false;
        }
        std::cout << when << ": " << boost::json::serialize(now->value) << '\n';
        return true;
    };
    const xstate::event remind{.type = "remind", .payload = {}};
    const xstate::event dismiss{.type = "dismiss", .payload = {}};

    if (!system.send(*actor, remind).has_value() || !system.send(*actor, dismiss).has_value() ||
        !system.clock_tick(1000).has_value() || !print_value("at 1000")) {
        return 1;
    }
    if (!system.send(*actor, remind).has_value() || !system.clock_tick(2000).has_value() ||
        !print_value("at 2000")) {
        return 1;
    }
    // end::timers[]
    return 0;
}
