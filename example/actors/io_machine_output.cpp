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

namespace xstate = webcpp::xstate;

int main() {
    // tag::output[]
    // ({ input }) => ({ amount: input.amount, currency: input.toCurrency })
    const xstate::context_maker from_input =
        [](const boost::json::value& input) -> xstate::result<boost::json::value> {
        const boost::json::object* given = input.if_object();
        const boost::json::value* amount =
            given == nullptr ? nullptr : given->if_contains("amount");
        const boost::json::value* currency =
            given == nullptr ? nullptr : given->if_contains("toCurrency");
        if (amount == nullptr || currency == nullptr) {
            return xstate::failure<boost::json::value>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"amount", *amount}, {"currency", *currency}};
    };
    // assign({ amount: ({ event }) => event.amount })
    const xstate::assigner keep_amount =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::value* amount = args.event.payload.if_contains("amount");
        if (amount == nullptr) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"amount", *amount}};
    };
    // ({ context }) => ({ amount: context.amount, currency: context.currency })
    const xstate::value_maker from_context =
        [](const xstate::action_args& args) -> xstate::result<std::optional<boost::json::value>> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* amount =
            context == nullptr ? nullptr : context->if_contains("amount");
        const boost::json::value* currency =
            context == nullptr ? nullptr : context->if_contains("currency");
        if (amount == nullptr || currency == nullptr) {
            return xstate::failure<std::optional<boost::json::value>>(
                xstate::errc::implementation_failed);
        }
        return boost::json::value(
            boost::json::object{{"amount", *amount}, {"currency", *currency}});
    };

    xstate::implementations implementations;
    implementations.context = from_input;
    implementations.actions.emplace("keepAmount", xstate::assign_action{.assignment = keep_amount});
    // The machine's output: the output of its root, whose id is currency.
    implementations.outputs.emplace("currency", from_context);
    const boost::json::value config = boost::json::parse(R"({
        "id": "currency",
        "initial": "converting",
        "states": {
            "converting": {
                "on": {
                    "amount.converted": {"target": "converted", "actions": "keepAmount"}
                }
            },
            "converted": {"type": "final"}
        }
    })");
    const xstate::result<xstate::machine> currency_machine =
        xstate::create_machine(config, implementations);
    if (!currency_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const boost::json::object input{{"amount", 10}, {"fromCurrency", "USD"}, {"toCurrency", "EUR"}};
    const xstate::result<xstate::actor_ref> currency_actor =
        system.create_actor(*currency_machine, {.input = input});
    if (!currency_actor.has_value()) {
        return 1;
    }
    const xstate::result<void> subscribed =
        system.subscribe(*currency_actor, [](const xstate::snapshot& snapshot) {
            if (snapshot.status == xstate::status::done && snapshot.output.has_value()) {
                std::cout << boost::json::serialize(*snapshot.output) << '\n';
            }
        });
    if (!subscribed.has_value()) {
        return 1;
    }
    const xstate::event converted{.type = "amount.converted", .payload = {{"amount", 12}}};
    if (!system.start(*currency_actor).has_value() ||
        !system.send(*currency_actor, converted).has_value()) {
        return 1;
    }
    // end::output[]
    return 0;
}
