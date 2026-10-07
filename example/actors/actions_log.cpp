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

int main() {
    // tag::machine[]
    const xstate::assigner add =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* count =
            context == nullptr ? nullptr : context->if_contains("count");
        if (count == nullptr || !count->is_int64()) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"count", count->get_int64() + 1}};
    };
    const xstate::value_maker message =
        [](const xstate::action_args&) -> xstate::result<std::optional<boost::json::value>> {
        return std::optional<boost::json::value>("incremented");
    };
    const xstate::value_maker count =
        [](const xstate::action_args& args) -> xstate::result<std::optional<boost::json::value>> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* found =
            context == nullptr ? nullptr : context->if_contains("count");
        if (found == nullptr) {
            return xstate::failure<std::optional<boost::json::value>>(
                xstate::errc::implementation_failed);
        }
        return std::optional<boost::json::value>(*found);
    };
    xstate::implementations implementations;
    implementations.actions.emplace("add", xstate::assign_action{.assignment = add});
    implementations.actions.emplace("logMessage",
                                    xstate::log_action{.value = message, .label = std::nullopt});
    implementations.actions.emplace("logCount",
                                    xstate::log_action{.value = count, .label = "count"});
    implementations.actions.emplace(
        "logAll", xstate::log_action{.value = std::nullopt, .label = std::nullopt});

    const boost::json::value config = boost::json::parse(R"({
        "context": {"count": 0},
        "on": {
            "increment": {"actions": ["add", "logMessage", "logCount", "logAll"]}
        }
    })");
    const xstate::result<xstate::machine> counter = xstate::create_machine(config, implementations);
    if (!counter.has_value()) {
        return 1;
    }
    // end::machine[]

    // tag::run[]
    const xstate::event increment{.type = "increment", .payload = {}};

    const xstate::snapshot initial = xstate::get_initial_snapshot(*counter);
    const auto [next, actions] = xstate::transition(*counter, initial, increment);
    for (const xstate::action& one : actions) {
        std::cout << one.type << ' ' << boost::json::serialize(one.params) << '\n';
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<void> logging = system.on_log(
        [](xstate::actor_ref, const boost::json::value* value, std::string_view label) {
            if (!label.empty()) {
                std::cout << label << ": ";
            }
            std::cout << (value == nullptr ? "undefined" : boost::json::serialize(*value)) << '\n';
        });
    if (!logging.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*counter);
    if (!actor.has_value() || !system.start(*actor).has_value() ||
        !system.send(*actor, increment).has_value()) {
        return 1;
    }
    // end::run[]
    return 0;
}
