// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>
#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>
#include <string_view>

namespace xstate = webcpp::xstate;

int main() {
    // tag::child[]
    // ({ input }) => input
    const xstate::context_maker the_input =
        [](const boost::json::value& input) -> xstate::result<boost::json::value> { return input; };
    // ({ context }) => ({ total: context.items * context.unitPrice })
    const xstate::value_maker total_of_context =
        [](const xstate::action_args& args) -> xstate::result<std::optional<boost::json::value>> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* items =
            context == nullptr ? nullptr : context->if_contains("items");
        const boost::json::value* unit_price =
            context == nullptr ? nullptr : context->if_contains("unitPrice");
        if (items == nullptr || unit_price == nullptr || !items->is_int64() ||
            !unit_price->is_int64()) {
            return xstate::failure<std::optional<boost::json::value>>(
                xstate::errc::implementation_failed);
        }
        return boost::json::value(
            boost::json::object{{"total", items->get_int64() * unit_price->get_int64()}});
    };

    xstate::implementations quote_implementations;
    quote_implementations.context = the_input;
    quote_implementations.outputs.emplace("quote", total_of_context);
    const boost::json::value quote_config = boost::json::parse(R"({
        "id": "quote",
        "initial": "priced",
        "states": {
            "priced": {"type": "final"}
        }
    })");
    const xstate::result<xstate::machine> quote_machine =
        xstate::create_machine(quote_config, quote_implementations);
    if (!quote_machine.has_value()) {
        return 1;
    }
    // end::child[]

    // tag::parent[]
    // ({ event }) => console.log(event), as a log of the event
    const xstate::value_maker the_event =
        [](const xstate::action_args& args) -> xstate::result<std::optional<boost::json::value>> {
        return boost::json::value(xstate::to_json(args.event));
    };
    // assign({ quote: ({ event }) => event.output })
    const xstate::assigner keep_quote =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::value* output = args.event.payload.if_contains("output");
        if (output == nullptr) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"quote", *output}};
    };

    xstate::implementations order_implementations;
    order_implementations.actors.emplace("quote", xstate::machine_actor{*quote_machine});
    order_implementations.actions.emplace(
        "logEvent", xstate::log_action{.value = the_event, .label = std::nullopt});
    order_implementations.actions.emplace("keepQuote",
                                          xstate::assign_action{.assignment = keep_quote});
    const boost::json::value order_config = boost::json::parse(R"({
        "id": "order",
        "context": {"quote": null},
        "initial": "quoting",
        "states": {
            "quoting": {
                "invoke": {
                    "id": "quote",
                    "src": "quote",
                    "input": {"items": 3, "unitPrice": 4},
                    "onDone": {"target": "quoted", "actions": ["logEvent", "keepQuote"]}
                }
            },
            "quoted": {}
        }
    })");
    const xstate::result<xstate::machine> order_machine =
        xstate::create_machine(order_config, order_implementations);
    if (!order_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<void> logging =
        system.on_log([](xstate::actor_ref, const boost::json::value* value, std::string_view) {
            if (value != nullptr) {
                std::cout << boost::json::serialize(*value) << '\n';
            }
        });
    if (!logging.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> order = system.create_actor(*order_machine);
    if (!order.has_value() || !system.start(*order).has_value()) {
        return 1;
    }
    const xstate::snapshot* quoted = system.snapshot_of(*order);
    if (quoted == nullptr) {
        return 1;
    }
    std::cout << boost::json::serialize(quoted->value) << ' '
              << boost::json::serialize(quoted->context) << '\n';
    // end::parent[]
    return 0;
}
