// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace xstate = webcpp::xstate;
namespace xactor = webcpp::xactor;

namespace {

/** An assign that adds one to the context's member `key`. */
xstate::assign_action counting(std::string key) {
    return xstate::assign_action{
        .assignment = [key = std::move(key)](
                          const xstate::action_args& args) -> xstate::result<boost::json::object> {
            const boost::json::value* count =
                args.context.is_object() ? args.context.get_object().if_contains(key) : nullptr;
            if (count == nullptr || !count->is_int64()) {
                return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
            }
            return boost::json::object{{key, count->get_int64() + 1}};
        },
    };
}

/** An event maker that always makes the event `type`. */
xstate::event_maker making(std::string type) {
    return [type = std::move(type)](const xstate::action_args&) -> xstate::result<xstate::event> {
        return xstate::event{.type = type, .payload = {}};
    };
}

std::string_view name_of(xactor::status status) {
    switch (status) {
        case xactor::status::active: return "active";
        case xactor::status::done: return "done";
        case xactor::status::error: return "error";
        case xactor::status::stopped: return "stopped";
    }
    return "unknown";
}

// tag::run[]
/** What each actor did in a run, by its id: every macrostep it settled, then its status. */
using trace = std::map<std::string, std::vector<std::string>, std::less<>>;

/** A run: its trace, and how many times the host had to resume it. */
struct run {
    trace actors;
    std::size_t resumes = 0;
};

/**
 Resumes a system until no actor is parked; how many resumes it took, or
 nothing when a call failed or it never settled.
*/
std::optional<std::size_t> settle(xstate::actor_system& system,
                                  xstate::result<xstate::run_outcome> outcome) {
    constexpr std::size_t most = 100'000;
    std::size_t resumes = 0;
    while (outcome.has_value() && *outcome == xstate::run_outcome::out_of_fuel && resumes < most) {
        outcome = system.resume();
        ++resumes;
    }
    if (!outcome.has_value() || *outcome != xstate::run_outcome::settled) {
        return std::nullopt;
    }
    return resumes;
}

/** Runs the test's script on a system whose executions have `fuel` each. */
std::optional<run> run_with(const xstate::machine& app, std::uint32_t fuel) {
    xstate::actor_system system({.fuel = fuel});
    run made;
    std::vector<xstate::actor_ref> started;
    const xstate::result<void> inspecting = system.inspect(xstate::inspector{
        .started = [&started](xstate::actor_ref actor) { started.push_back(actor); },
        .settled =
            [&system, &made](xstate::actor_ref actor, const xstate::machine& /*logic*/,
                             const xstate::event& cause, const xstate::snapshot& settled) {
                made.actors[std::string(system.id_of(actor))].push_back(
                    cause.type + " -> " + boost::json::serialize(settled.value) + ' ' +
                    boost::json::serialize(settled.context));
            },
        .emitted = {},
    });
    if (!inspecting.has_value()) {
        return std::nullopt;
    }
    const xstate::result<xstate::actor_ref> root =
        system.create_actor(app, {.input = nullptr, .id = "app", .system_id = std::nullopt});
    if (!root.has_value()) {
        return std::nullopt;
    }

    const xstate::event go{.type = "go", .payload = {}};
    const std::vector<std::function<xstate::result<xstate::run_outcome>()>> script{
        [&] { return system.start(*root); },
        [&] { return system.send(*root, go); },
        [&] { return system.send(*root, go); },
        [&] { return system.clock_tick(1000); },
    };
    for (const auto& call : script) {
        const std::optional<std::size_t> resumes = settle(system, call());
        if (!resumes.has_value()) {
            return std::nullopt;
        }
        made.resumes += *resumes;
    }
    for (const xstate::actor_ref actor : started) {
        const xstate::result<xactor::status> status = system.status_of(actor);
        if (!status.has_value()) {
            return std::nullopt;
        }
        made.actors[std::string(system.id_of(actor))].push_back("status " +
                                                                std::string(name_of(*status)));
    }
    return made;
}

// end::run[]

}  // namespace

int main() {
    // tag::machines[]
    xstate::implementations worker_implementations;
    worker_implementations.actions.emplace("countPing", counting("pings"));
    worker_implementations.actions.emplace("pong", xstate::send_parent_action{
                                                       .event = making("pong"),
                                                       .id = std::nullopt,
                                                       .delay = std::nullopt,
                                                   });
    const boost::json::value worker_config = boost::json::parse(R"({
        "id": "worker",
        "context": {"pings": 0},
        "on": {"ping": {"actions": ["countPing", "pong"]}}
    })");
    const xstate::result<xstate::machine> worker =
        xstate::create_machine(worker_config, worker_implementations);
    if (!worker.has_value()) {
        return 1;
    }

    xstate::implementations app_implementations;
    app_implementations.actors.emplace("worker", xstate::machine_actor{*worker});
    app_implementations.actions.emplace("countPong", counting("pongs"));
    app_implementations.actions.emplace("ping", xstate::send_to_action{
                                                    .target = "worker",
                                                    .event = making("ping"),
                                                    .id = std::nullopt,
                                                    .delay = std::nullopt,
                                                });
    const boost::json::value app_config = boost::json::parse(R"({
        "id": "app",
        "initial": "working",
        "context": {"pongs": 0},
        "states": {
            "working": {
                "invoke": {"src": "worker", "id": "worker"},
                "on": {"go": {"actions": "ping"}, "pong": {"actions": "countPong"}},
                "after": {"1000": "finished"}
            },
            "finished": {"type": "final"}
        }
    })");
    const xstate::result<xstate::machine> app =
        xstate::create_machine(app_config, app_implementations);
    if (!app.has_value()) {
        return 1;
    }
    // end::machines[]

    // tag::test[]
    const std::optional<run> plenty = run_with(*app, 10'000);
    if (!plenty.has_value() || plenty->resumes != 0) {
        return 1;
    }
    for (const auto& [id, entries] : plenty->actors) {
        for (const std::string& entry : entries) {
            std::cout << id << ": " << entry << '\n';
        }
    }

    for (std::uint32_t fuel = 1; fuel <= 40; ++fuel) {
        const std::optional<run> scarce = run_with(*app, fuel);
        if (!scarce.has_value() || scarce->actors != plenty->actors) {
            std::cout << "fuel " << fuel << " differs\n";
            return 1;
        }
        if (fuel == 1) {
            std::cout << "fuel 1: the same, after " << scarce->resumes << " resumes\n";
        }
    }
    std::cout << "every budget from 1 to 40: the same\n";
    // end::test[]
    return 0;
}
