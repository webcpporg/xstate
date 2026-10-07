// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <iostream>

namespace xstate = webcpp::xstate;

namespace {

/** The context's count plus `step`, as an assign merges it into the context. */
xstate::result<boost::json::object> add(const xstate::action_args& args, std::int64_t step) {
    const boost::json::object* context = args.context.if_object();
    const boost::json::value* count = context == nullptr ? nullptr : context->if_contains("count");
    const std::int64_t* value = count == nullptr ? nullptr : count->if_int64();
    if (value == nullptr) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"count", *value + step}};
}

}  // namespace

int main() {
    // tag::provide[]
    const boost::json::value config = boost::json::parse(R"({
        "context": {"count": 0},
        "on": {
            "inc": {"actions": "increment"}
        }
    })");

    const xstate::assign_action plus_one{
        .assignment = [](const xstate::action_args& args) { return add(args, 1); },
    };
    xstate::implementations adds_one;
    adds_one.actions.emplace("increment", plus_one);
    const xstate::result<xstate::machine> counter = xstate::create_machine(config, adds_one);
    if (!counter.has_value()) {
        return 1;
    }

    const xstate::assign_action plus_ten{
        .assignment = [](const xstate::action_args& args) { return add(args, 10); },
    };
    xstate::implementations adds_ten = counter->registry();
    adds_ten.actions.insert_or_assign("increment", plus_ten);
    const xstate::result<xstate::machine> by_ten = xstate::create_machine(config, adds_ten);
    if (!by_ten.has_value()) {
        return 1;
    }

    const auto increment_once = [](const xstate::machine& machine) {
        const xstate::snapshot initial = xstate::initial_transition(machine).first;
        const xstate::event inc{.type = "inc", .payload = {}};
        return xstate::transition(machine, initial, inc).first.context;
    };
    std::cout << "counter " << boost::json::serialize(increment_once(*counter)) << '\n';
    std::cout << "byTen " << boost::json::serialize(increment_once(*by_ten)) << '\n';
    // end::provide[]
    return 0;
}
