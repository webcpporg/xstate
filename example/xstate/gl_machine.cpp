// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

// tag::helpers[]
/** The count of items an order's context holds, or none when it holds no integer `items`. */
std::optional<std::int64_t> items_of(const boost::json::value& context) {
    const boost::json::object* members = context.if_object();
    const boost::json::value* items = members == nullptr ? nullptr : members->if_contains("items");
    if (items == nullptr || !items->is_int64()) {
        return std::nullopt;
    }
    return items->get_int64();
}

std::string_view status_name(xstate::status status) {
    switch (status) {
        case xstate::status::active: return "active";
        case xstate::status::done: return "done";
        case xstate::status::error: return "error";
    }
    return "unknown";
}

/** Prints where a microstep left the machine and the actions it returned. */
void print(std::string_view label, std::size_t number, const xstate::progress& made) {
    const xstate::snapshot& snapshot = made.step.snapshot;
    std::cout << label << " #" << number << ": " << boost::json::serialize(snapshot.value) << ' '
              << boost::json::serialize(snapshot.context) << ' ' << status_name(snapshot.status)
              << " [";
    std::string_view separator;
    for (const xstate::action& returned : made.step.actions) {
        std::cout << separator << returned.type;
        separator = ", ";
    }
    std::cout << ']' << (made.settled ? " settled" : "") << '\n';
}

// end::helpers[]

}  // namespace

int main() {
    // tag::machine[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "order",
        "initial": "cart",
        "context": {"items": 0},
        "states": {
            "cart": {
                "on": {
                    "add": {"actions": "count"},
                    "checkout": {"target": "checking"}
                }
            },
            "checking": {
                "always": [{"target": "paying", "guard": "hasItems"}, {"target": "cart"}]
            },
            "paying": {"entry": ["charge", "confirm"], "on": {"paid": "closed"}},
            "closed": {"type": "final"}
        }
    })");

    const xstate::assigner count =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const std::optional<std::int64_t> items = items_of(args.context);
        if (!items.has_value()) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"items", *items + 1}};
    };
    const xstate::event_maker paid =
        [](const xstate::action_args&) -> xstate::result<xstate::event> {
        return xstate::event{.type = "paid", .payload = {}};
    };
    const xstate::predicate has_items =
        [](const xstate::action_args& args) -> xstate::result<bool> {
        const std::optional<std::int64_t> items = items_of(args.context);
        if (!items.has_value()) {
            return xstate::failure<bool>(xstate::errc::implementation_failed);
        }
        return *items > 0;
    };

    xstate::implementations implementations;
    implementations.actions.emplace("count", xstate::assign_action{.assignment = count});
    implementations.actions.emplace(
        "confirm", xstate::raise_action{.event = paid, .id = std::nullopt, .delay = std::nullopt});
    implementations.guards.emplace("hasItems", has_items);

    const xstate::result<xstate::machine> order = xstate::create_machine(config, implementations);
    if (!order.has_value()) {
        return 1;
    }

    xstate::snapshot now = xstate::get_initial_snapshot(*order);
    std::cout << "initial: " << boost::json::serialize(now.value) << ' '
              << boost::json::serialize(now.context) << ' ' << status_name(now.status) << '\n';
    for (const std::string_view type : {"checkout", "add", "checkout"}) {
        xstate::macrostep step =
            xstate::begin(*order, now, xstate::event{.type = std::string(type), .payload = {}});
        std::size_t number = 0;
        while (!step.done()) {
            const xstate::progress made = step.next();
            print(type, ++number, made);
        }
        now = step.result().snapshot;
    }
    // end::machine[]
    return 0;
}
