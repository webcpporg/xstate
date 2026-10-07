// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// tag::program[]
#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <iostream>
#include <utility>

namespace xstate = webcpp::xstate;

namespace {

/** An assign that adds `step` to the context's count. */
xstate::assign_action adding(std::int64_t step) {
    auto add = [step](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::system::result<const boost::json::value&> count = args.context.try_at("count");
        if (!count.has_value()) {
            return count.error();
        }
        const boost::system::result<std::int64_t> number = count->try_as_int64();
        if (!number.has_value()) {
            return number.error();
        }
        return boost::json::object{{"count", *number + step}};
    };
    return xstate::assign_action{.assignment = std::move(add)};
}

/** An assign that sets the count to the event's `value`. */
xstate::result<boost::json::object> set_count(const xstate::action_args& args) {
    const boost::json::value* value = args.event.payload.if_contains("value");
    if (value == nullptr) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"count", *value}};
}

}  // namespace

int main() {
    const boost::json::value config = boost::json::parse(R"({
        "context": {"count": 0},
        "on": {
            "INC": {"actions": "increment"},
            "DEC": {"actions": "decrement"},
            "SET": {"actions": "set"}
        }
    })");
    xstate::implementations implementations;
    implementations.actions.emplace("increment", adding(1));
    implementations.actions.emplace("decrement", adding(-1));
    implementations.actions.emplace("set", xstate::assign_action{.assignment = set_count});
    const xstate::result<xstate::machine> count_machine =
        xstate::create_machine(config, std::move(implementations));
    if (!count_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 100});
    const xstate::result<xstate::actor_ref> count_actor = system.create_actor(*count_machine);
    if (!count_actor.has_value()) {
        return 1;
    }
    if (!system.start(*count_actor).has_value()) {
        return 1;
    }

    const xstate::result<void> subscribed =
        system.subscribe(*count_actor, [](const xstate::snapshot& state) {
            const boost::system::result<const boost::json::value&> count =
                state.context.try_at("count");
            if (count.has_value()) {
                std::cout << boost::json::serialize(*count) << '\n';
            }
        });
    if (!subscribed.has_value()) {
        return 1;
    }

    const xstate::event inc{.type = "INC", .payload = {}};
    const xstate::event dec{.type = "DEC", .payload = {}};
    const xstate::event set{.type = "SET", .payload = {{"value", 10}}};

    if (!system.send(*count_actor, inc).has_value()) {
        return 1;
    }
    // prints 1
    if (!system.send(*count_actor, dec).has_value()) {
        return 1;
    }
    // prints 0
    if (!system.send(*count_actor, set).has_value()) {
        return 1;
    }
    // prints 10
    return 0;
}

// end::program[]
