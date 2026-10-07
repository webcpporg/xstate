// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>
#include <string>

namespace xstate = webcpp::xstate;

namespace {

// tag::run[]
/** Pays a checkout and returns its output: "error" when it is not done, "none" without one. */
std::string checkout_output(const xstate::machine& checkout) {
    const xstate::event paid{.type = "paid", .payload = {{"receiptId", "R-1"}}};
    const xstate::snapshot start = xstate::initial_transition(checkout).first;
    const xstate::snapshot done = xstate::transition(checkout, start, paid).first;
    if (done.status != xstate::status::done) {
        return "error";
    }
    if (!done.output.has_value()) {
        return "none";
    }
    return boost::json::serialize(*done.output);
}

// end::run[]

}  // namespace

int main() {
    // tag::machines[]
    // ({ event }) => ({ receiptId: event.receiptId })
    const xstate::value_maker receipt_of_event =
        [](const xstate::action_args& args) -> xstate::result<std::optional<boost::json::value>> {
        const boost::json::value* receipt_id = args.event.payload.if_contains("receiptId");
        if (receipt_id == nullptr) {
            return xstate::failure<std::optional<boost::json::value>>(
                xstate::errc::implementation_failed);
        }
        return boost::json::value(boost::json::object{{"receiptId", *receipt_id}});
    };
    // ({ event }) => event
    const xstate::value_maker the_event =
        [](const xstate::action_args& args) -> xstate::result<std::optional<boost::json::value>> {
        return boost::json::value(xstate::to_json(args.event));
    };
    const boost::json::value config = boost::json::parse(R"({
        "id": "checkout",
        "initial": "paying",
        "states": {
            "paying": {"on": {"paid": "receipt"}},
            "receipt": {"type": "final"}
        }
    })");

    xstate::implementations without_root_output;
    without_root_output.outputs.emplace("checkout.receipt", receipt_of_event);
    xstate::implementations with_root_output = without_root_output;
    with_root_output.outputs.emplace("checkout", the_event);

    const xstate::result<xstate::machine> with_output =
        xstate::create_machine(config, with_root_output);
    const xstate::result<xstate::machine> without_output =
        xstate::create_machine(config, without_root_output);
    if (!with_output.has_value() || !without_output.has_value()) {
        return 1;
    }
    std::cout << checkout_output(*with_output) << '\n' << checkout_output(*without_output) << '\n';
    // end::machines[]
    return 0;
}
