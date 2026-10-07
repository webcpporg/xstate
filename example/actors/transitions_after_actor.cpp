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
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

/** The integer `object` holds under `key`, or none when it holds no integer there. */
std::optional<std::int64_t> integer_at(const boost::json::value& object, std::string_view key) {
    const boost::json::object* members = object.if_object();
    const boost::json::value* found = members == nullptr ? nullptr : members->if_contains(key);
    if (found == nullptr || !found->is_int64()) {
        return std::nullopt;
    }
    return found->get_int64();
}

}  // namespace

int main() {
    // tag::implementations[]
    xstate::implementations implementations;
    implementations.delays.emplace(
        "timeout", [](const xstate::action_args& args) -> xstate::result<std::uint64_t> {
            const std::optional<std::int64_t> attempts = integer_at(args.context, "attempts");
            if (!attempts.has_value() || *attempts < 0) {
                return xstate::failure<std::uint64_t>(xstate::errc::implementation_failed);
            }
            return static_cast<std::uint64_t>(*attempts) * 1000;
        });
    implementations.guards.emplace(
        "canRetry", [](const xstate::action_args& args) -> xstate::result<bool> {
            const std::optional<std::int64_t> attempts = integer_at(args.context, "attempts");
            if (!attempts.has_value()) {
                return xstate::failure<bool>(xstate::errc::implementation_failed);
            }
            return *attempts < 3;
        });
    implementations.actions.emplace(
        "countAttempt",
        xstate::assign_action{
            .assignment =
                [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
                const std::optional<std::int64_t> attempts = integer_at(args.context, "attempts");
                if (!attempts.has_value()) {
                    return xstate::failure<boost::json::object>(
                        xstate::errc::implementation_failed);
                }
                return boost::json::object{{"attempts", *attempts + 1}};
            },
        });
    // end::implementations[]

    // tag::config[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "retry",
        "initial": "attempting",
        "context": {"attempts": 1},
        "states": {
            "attempting": {
                "after": {
                    "timeout": [
                        {
                            "guard": "canRetry",
                            "actions": "countAttempt",
                            "target": "attempting",
                            "reenter": true
                        },
                        {"target": "failed"}
                    ]
                }
            },
            "failed": {}
        }
    })");
    const xstate::result<xstate::machine> retry = xstate::create_machine(config, implementations);
    if (!retry.has_value()) {
        return 1;
    }
    // end::config[]

    // tag::run[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*retry);
    if (!actor.has_value() || !system.start(*actor).has_value()) {
        return 1;
    }
    for (const std::uint64_t now : {0, 1000, 2999, 3000, 6000}) {
        if (!system.clock_tick(now).has_value()) {
            return 1;
        }
        const xstate::snapshot* state = system.snapshot_of(*actor);
        if (state == nullptr) {
            return 1;
        }
        std::cout << "at " << now << ": " << boost::json::serialize(state->value) << ' '
                  << boost::json::serialize(state->context) << '\n';
    }
    // end::run[]
    return 0;
}
