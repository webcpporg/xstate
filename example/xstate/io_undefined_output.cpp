// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <array>
#include <iostream>
#include <optional>
#include <string>

namespace xstate = webcpp::xstate;

namespace {

// tag::output[]
/** A snapshot's output: "error" when it is not done, "none" when it has none. */
std::string output_of(const xstate::snapshot& done) {
    if (done.status != xstate::status::done) {
        return "error";
    }
    if (!done.output.has_value()) {
        return "none";
    }
    return boost::json::serialize(*done.output);
}

// end::output[]

}  // namespace

int main() {
    // tag::undefined[]
    // ({ input }) => input
    const xstate::context_maker the_input =
        [](const boost::json::value& input) -> xstate::result<boost::json::value> { return input; };
    // ({ context }) => context.total, undefined when the context has no total
    const xstate::value_maker total_of_context =
        [](const xstate::action_args& args) -> xstate::result<std::optional<boost::json::value>> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* total =
            context == nullptr ? nullptr : context->if_contains("total");
        if (total == nullptr) {
            return std::optional<boost::json::value>();
        }
        return std::optional<boost::json::value>(*total);
    };

    xstate::implementations implementations;
    implementations.context = the_input;
    implementations.outputs.emplace("order", total_of_context);
    const boost::json::value config = boost::json::parse(R"({
        "id": "order",
        "initial": "placed",
        "states": {
            "placed": {"type": "final"}
        }
    })");
    const xstate::result<xstate::machine> order_machine =
        xstate::create_machine(config, implementations);
    if (!order_machine.has_value()) {
        return 1;
    }

    const std::array<boost::json::object, 3> inputs{
        boost::json::object{{"total", 42}},
        boost::json::object{},
        boost::json::object{{"total", nullptr}},
    };
    for (const boost::json::object& input : inputs) {
        const xstate::snapshot done = xstate::initial_transition(*order_machine, input).first;
        std::cout << boost::json::serialize(input) << " -> " << output_of(done) << '\n';
    }
    // end::undefined[]
    return 0;
}
