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

/** The integer member `key` of a JSON object, or nothing. */
std::optional<std::int64_t> integer_at(const boost::json::object* from, std::string_view key) {
    const boost::json::value* found = from == nullptr ? nullptr : from->if_contains(key);
    if (found == nullptr || !found->is_int64()) {
        return std::nullopt;
    }
    return found->get_int64();
}

}  // namespace

int main() {
    // tag::child[]
    const xstate::assigner add =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const std::optional<std::int64_t> count = integer_at(args.context.if_object(), "count");
        if (!count.has_value()) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"count", *count + 1}};
    };
    const xstate::assigner set_count =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const std::optional<std::int64_t> count = integer_at(&args.event.payload, "count");
        if (!count.has_value()) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"count", *count}};
    };
    const xstate::event_maker counted =
        [](const xstate::action_args& args) -> xstate::result<xstate::event> {
        const std::optional<std::int64_t> count = integer_at(args.context.if_object(), "count");
        if (!count.has_value()) {
            return xstate::failure<xstate::event>(xstate::errc::implementation_failed);
        }
        return xstate::event{.type = "counted", .payload = {{"count", *count}}};
    };
    xstate::implementations counter_implementations;
    counter_implementations.actions.emplace("add", xstate::assign_action{.assignment = add});
    counter_implementations.actions.emplace("setCount",
                                            xstate::assign_action{.assignment = set_count});
    counter_implementations.actions.emplace(
        "report",
        xstate::send_parent_action{.event = counted, .id = std::nullopt, .delay = std::nullopt});

    const boost::json::value counter_config = boost::json::parse(R"({
        "id": "counter",
        "context": {"count": 0},
        "on": {
            "inc": {"actions": ["add", "report"]},
            "set": {"actions": ["setCount", "report"]}
        }
    })");
    const xstate::result<xstate::machine> counter =
        xstate::create_machine(counter_config, counter_implementations);
    if (!counter.has_value()) {
        return 1;
    }
    // end::child[]

    // tag::parent[]
    const xstate::event_maker inc =
        [](const xstate::action_args&) -> xstate::result<xstate::event> {
        return xstate::event{.type = "inc", .payload = {}};
    };
    const xstate::assigner keep_last =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const std::optional<std::int64_t> count = integer_at(&args.event.payload, "count");
        if (!count.has_value()) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"last", *count}};
    };
    const xstate::send_to_action send_inc{
        .target = "counter",
        .event = inc,
        .id = std::nullopt,
        .delay = std::nullopt,
    };
    const xstate::forward_to_action forward_set{
        .target = "counter",
        .id = std::nullopt,
        .delay = std::nullopt,
    };
    xstate::implementations app_implementations;
    app_implementations.actors.emplace("counter", xstate::machine_actor{*counter});
    app_implementations.actions.emplace("inc", send_inc);
    app_implementations.actions.emplace("forwardSet", forward_set);
    app_implementations.actions.emplace("keepLast", xstate::assign_action{.assignment = keep_last});

    const boost::json::value app_config = boost::json::parse(R"({
        "id": "app",
        "context": {"last": null},
        "invoke": {"id": "counter", "src": "counter"},
        "on": {
            "click": {"actions": "inc"},
            "set": {"actions": "forwardSet"},
            "counted": {"actions": "keepLast"}
        }
    })");
    const xstate::result<xstate::machine> app =
        xstate::create_machine(app_config, app_implementations);
    if (!app.has_value()) {
        return 1;
    }
    // end::parent[]

    // tag::run[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> app_actor = system.create_actor(*app);
    if (!app_actor.has_value() || !system.start(*app_actor).has_value()) {
        return 1;
    }
    const xstate::event click{.type = "click", .payload = {}};
    const xstate::event set{.type = "set", .payload = {{"count", 10}}};
    for (const xstate::event& sent : {click, click, set}) {
        if (!system.send(*app_actor, sent).has_value()) {
            return 1;
        }
        const std::optional<xstate::actor_ref> child = system.child_of(*app_actor, "counter");
        const xstate::snapshot* parent_now = system.snapshot_of(*app_actor);
        const xstate::snapshot* child_now =
            child.has_value() ? system.snapshot_of(*child) : nullptr;
        if (parent_now == nullptr || child_now == nullptr) {
            return 1;
        }
        std::cout << sent.type << ": app " << boost::json::serialize(parent_now->context)
                  << ", counter " << boost::json::serialize(child_now->context) << '\n';
    }
    // end::run[]
    return 0;
}
