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

// tag::assign[]
/** One more treat: the partial context `eatTreat` merges into the context. */
xstate::result<boost::json::object> eat_treat(const xstate::action_args& args) {
    const boost::json::object* context = args.context.if_object();
    const boost::json::value* treats =
        context == nullptr ? nullptr : context->if_contains("treats");
    if (treats == nullptr || !treats->is_int64()) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"treats", treats->get_int64() + 1}};
}

// end::assign[]

void print(const xstate::snapshot& now) {
    std::cout << boost::json::serialize(now.value) << ' ' << boost::json::serialize(now.context)
              << '\n';
}

}  // namespace

int main() {
    // tag::example[]
    xstate::implementations implementations;
    implementations.actions.emplace("eatTreat", xstate::assign_action{.assignment = eat_treat});

    const boost::json::value config = boost::json::parse(R"({
        "id": "dog",
        "initial": "begging",
        "context": {"treats": 0},
        "states": {
            "begging": {"on": {"gets treat": {"actions": "eatTreat"}}}
        }
    })");
    const xstate::result<xstate::machine> dog = xstate::create_machine(config, implementations);
    if (!dog.has_value()) {
        return 1;
    }

    xstate::snapshot now = xstate::get_initial_snapshot(*dog);
    print(now);
    const xstate::event gets_treat{.type = "gets treat", .payload = {}};
    for (int treat = 0; treat < 2; ++treat) {
        now = xstate::get_next_snapshot(*dog, now, gets_treat);
        print(now);
    }
    // end::example[]
    return 0;
}
