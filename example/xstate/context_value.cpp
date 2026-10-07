// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

namespace {

// tag::functions[]
xstate::result<boost::json::object> increment(const xstate::action_args& args) {
    const boost::json::object* context = args.context.if_object();
    const boost::json::value* count = context == nullptr ? nullptr : context->if_contains("count");
    if (count == nullptr || !count->is_int64()) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"count", count->get_int64() + 1}};
}

xstate::result<boost::json::object> set_count(const xstate::action_args& args) {
    const boost::json::value* count = args.event.payload.if_contains("count");
    if (count == nullptr || !count->is_int64()) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"count", *count}};
}

// end::functions[]
}  // namespace

int main() {
    // tag::value[]
    xstate::implementations implementations;
    implementations.actions.emplace("increment", xstate::assign_action{.assignment = increment});
    implementations.actions.emplace("setCount", xstate::assign_action{.assignment = set_count});
    const boost::json::value config = boost::json::parse(R"({
        "id": "counter",
        "context": {"count": 0},
        "on": {
            "inc": {"actions": "increment"},
            "set": {"actions": ["increment", "setCount"]}
        }
    })");
    const xstate::result<xstate::machine> counter = xstate::create_machine(config, implementations);
    if (!counter.has_value()) {
        return 1;
    }

    const xstate::event inc{.type = "inc", .payload = {}};
    const xstate::snapshot first = xstate::get_initial_snapshot(*counter);
    const xstate::snapshot second = xstate::get_next_snapshot(*counter, first, inc);
    const xstate::snapshot again = xstate::get_next_snapshot(*counter, first, inc);
    std::cout << boost::json::serialize(first.context) << ' '
              << boost::json::serialize(second.context) << ' '
              << boost::json::serialize(again.context) << '\n';
    // end::value[]

    // tag::failure[]
    const xstate::snapshot failed =
        xstate::get_next_snapshot(*counter, second, xstate::event{.type = "set", .payload = {}});
    std::cout << (failed.status == xstate::status::error ? "error " : "active ")
              << boost::json::serialize(failed.context) << '\n';
    // end::failure[]
    return 0;
}
