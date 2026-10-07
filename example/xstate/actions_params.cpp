// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    // tag::params[]
    const xstate::assigner add_rating =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::object* params = args.params.if_object();
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* by = params == nullptr ? nullptr : params->if_contains("by");
        const boost::json::value* rating =
            context == nullptr ? nullptr : context->if_contains("rating");
        if (by == nullptr || !by->is_int64() || rating == nullptr || !rating->is_int64()) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"rating", rating->get_int64() + by->get_int64()}};
    };
    xstate::implementations implementations;
    implementations.actions.emplace("addRating", xstate::assign_action{.assignment = add_rating});

    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "question",
        "context": {"rating": 0},
        "states": {
            "question": {
                "on": {
                    "feedback.good": {
                        "target": "thanks",
                        "actions": [
                            {"type": "track", "params": {"response": "good"}},
                            {"type": "addRating", "params": {"by": 5}}
                        ]
                    }
                }
            },
            "thanks": {"entry": "showConfetti"}
        }
    })");
    const xstate::result<xstate::machine> feedback =
        xstate::create_machine(config, implementations);
    if (!feedback.has_value()) {
        return 1;
    }

    const xstate::snapshot question = xstate::get_initial_snapshot(*feedback);
    const auto [thanks, actions] =
        xstate::transition(*feedback, question, {.type = "feedback.good", .payload = {}});
    for (const xstate::action& one : actions) {
        std::cout << one.type;
        if (!one.params.is_null()) {
            std::cout << ' ' << boost::json::serialize(one.params);
        }
        std::cout << '\n';
    }
    std::cout << "context " << boost::json::serialize(thanks.context) << '\n';
    // end::params[]
    return 0;
}
