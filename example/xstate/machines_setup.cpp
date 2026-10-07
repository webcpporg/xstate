// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

// tag::add[]
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

// end::add[]

}  // namespace

int main() {
    // tag::setup[]
    const xstate::assign_action increment{
        .assignment = [](const xstate::action_args& args) { return add(args, 1); },
    };
    const xstate::assign_action decrement{
        .assignment = [](const xstate::action_args& args) { return add(args, -1); },
    };
    xstate::implementations implementations;
    implementations.actions.emplace("increment", increment);
    implementations.actions.emplace("decrement", decrement);

    const boost::json::value config = boost::json::parse(R"({
        "context": {"count": 0},
        "on": {
            "inc": {"actions": "increment"},
            "dec": {"actions": "decrement"}
        }
    })");
    const xstate::result<xstate::machine> counter = xstate::create_machine(config, implementations);
    if (!counter.has_value()) {
        return 1;
    }

    xstate::snapshot now = xstate::initial_transition(*counter).first;
    for (const std::string_view type : {"inc", "inc", "dec"}) {
        const xstate::event sent{.type = std::string(type), .payload = {}};
        now = xstate::transition(*counter, now, sent).first;
        std::cout << type << ' ' << boost::json::serialize(now.context) << '\n';
    }
    // end::setup[]
    return 0;
}
