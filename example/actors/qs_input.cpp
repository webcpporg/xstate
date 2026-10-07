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

/** The integer `key` holds in the object `json`; none when it holds no integer. */
std::optional<std::int64_t> integer_at(const boost::json::value& json, std::string_view key) {
    const boost::json::object* object = json.if_object();
    if (object == nullptr) {
        return std::nullopt;
    }
    const boost::json::value* member = object->if_contains(key);
    if (member == nullptr || !member->is_int64()) {
        return std::nullopt;
    }
    return member->get_int64();
}

}  // namespace

int main() {
    // tag::program[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "toggle",
        "initial": "Inactive",
        "states": {
            "Inactive": {
                "on": {
                    "toggle": {"guard": "countBelowMax", "target": "Active"}
                }
            },
            "Active": {
                "entry": "incrementCount",
                "on": {"toggle": "Inactive"},
                "after": {"2000": "Inactive"}
            }
        }
    })");

    xstate::implementations implementations;
    implementations.context =
        [](const boost::json::value& input) -> xstate::result<boost::json::value> {
        const std::optional<std::int64_t> max_count = integer_at(input, "maxCount");
        if (!max_count.has_value()) {
            return xstate::failure<boost::json::value>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"count", 0}, {"maxCount", *max_count}};
    };
    implementations.guards.emplace(
        "countBelowMax", [](const xstate::action_args& args) -> xstate::result<bool> {
            const std::optional<std::int64_t> count = integer_at(args.context, "count");
            const std::optional<std::int64_t> max_count = integer_at(args.context, "maxCount");
            if (!count.has_value() || !max_count.has_value()) {
                return xstate::failure<bool>(xstate::errc::implementation_failed);
            }
            return *count < *max_count;
        });
    implementations.actions.emplace(
        "incrementCount",
        xstate::assign_action{
            .assignment =
                [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
                const std::optional<std::int64_t> count = integer_at(args.context, "count");
                if (!count.has_value()) {
                    return xstate::failure<boost::json::object>(
                        xstate::errc::implementation_failed);
                }
                return boost::json::object{{"count", *count + 1}};
            },
        });

    const xstate::result<xstate::machine> toggle_machine =
        xstate::create_machine(config, implementations);
    if (!toggle_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 1000});
    const auto settled = [](const xstate::result<xstate::run_outcome>& ran) {
        return ran.has_value() && *ran == xstate::run_outcome::settled;
    };

    const xstate::actor_options options{
        .input = boost::json::object{{"maxCount", 1}},
        .id = std::nullopt,
        .system_id = std::nullopt,
    };
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*toggle_machine, options);
    if (!actor.has_value()) {
        return 1;
    }

    const xstate::result<void> subscribed =
        system.subscribe(*actor, [](const xstate::snapshot& snapshot) {
            std::cout << "State: " << boost::json::serialize(snapshot.value) << ' '
                      << boost::json::serialize(snapshot.context) << '\n';
        });
    if (!subscribed.has_value()) {
        return 1;
    }

    const xstate::event toggle{.type = "toggle", .payload = {}};
    if (!settled(system.start(*actor))) {
        return 1;
    }
    if (!settled(system.send(*actor, toggle))) {  // Active, count 1
        return 1;
    }
    if (!settled(system.clock_tick(2000))) {  // after 2000 ms: Inactive
        return 1;
    }
    if (!settled(system.send(*actor, toggle))) {  // count is not below maxCount: stays
        return 1;
    }
    // end::program[]
    return 0;
}
