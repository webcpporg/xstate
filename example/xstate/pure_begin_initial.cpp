// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

/** The types of `actions`, as a list: [a, b]. */
std::string types_of(const std::vector<xstate::action>& actions) {
    std::string listed = "[";
    for (const xstate::action& one : actions) {
        if (listed.size() > 1) {
            listed += ", ";
        }
        listed += one.type;
    }
    return listed + "]";
}

/** The transitions a microstep took, as source -> targets, or "none". */
std::string transitions_of(const xstate::machine& owner, const xstate::microstep& micro) {
    if (!micro.transitions.has_value() || micro.transitions->empty()) {
        return "none";
    }
    std::string listed;
    for (const xstate::transition_definition* one : *micro.transitions) {
        if (!listed.empty()) {
            listed += ", ";
        }
        listed += owner.node(one->source).key + " ->";
        for (const std::size_t target : one->target.value_or(std::vector<std::size_t>{})) {
            listed += ' ' + owner.node(target).key;
        }
    }
    return listed;
}

}  // namespace

int main() {
    // tag::example[]
    const xstate::context_maker from_input =
        [](const boost::json::value& input) -> xstate::result<boost::json::value> {
        const boost::json::object* given = input.if_object();
        const boost::json::value* user = given == nullptr ? nullptr : given->if_contains("user");
        return boost::json::object{{"user", user == nullptr ? boost::json::value() : *user}};
    };
    const xstate::predicate has_user = [](const xstate::action_args& args) -> xstate::result<bool> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* user =
            context == nullptr ? nullptr : context->if_contains("user");
        return user != nullptr && !user->is_null();
    };
    xstate::implementations implementations;
    implementations.context = from_input;
    implementations.guards.emplace("hasUser", has_user);

    const boost::json::value config = boost::json::parse(R"({
        "id": "session",
        "initial": "checking",
        "states": {
            "checking": {
                "always": [
                    {"target": "signedIn", "guard": "hasUser"},
                    {"target": "signedOut"}
                ]
            },
            "signedIn": {"entry": "welcome"},
            "signedOut": {}
        }
    })");
    const xstate::result<xstate::machine> session = xstate::create_machine(config, implementations);
    if (!session.has_value()) {
        return 1;
    }

    const boost::json::value input = boost::json::object{{"user", "ana"}};
    xstate::macrostep cursor = xstate::begin_initial(*session, input);
    while (!cursor.done()) {
        const xstate::progress made = cursor.next();
        const xstate::microstep& micro = made.step;
        std::cout << boost::json::serialize(xstate::to_json(micro.event)) << ", "
                  << transitions_of(*session, micro) << ", " << types_of(micro.actions) << ", "
                  << boost::json::serialize(micro.snapshot.value)
                  << (made.settled ? ", settled" : "") << '\n';
    }
    std::cout << "context: " << boost::json::serialize(cursor.result().snapshot.context) << '\n';
    // end::example[]
    return 0;
}
