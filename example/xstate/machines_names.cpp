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

// tag::guard[]
/** Whether the context's role is "admin". */
xstate::result<bool> can_access(const xstate::action_args& args) {
    const boost::json::object* context = args.context.if_object();
    const boost::json::value* role = context == nullptr ? nullptr : context->if_contains("role");
    return role != nullptr && role->is_string() && role->get_string() == "admin";
}

// end::guard[]

}  // namespace

int main() {
    // tag::names[]
    xstate::implementations implementations;
    implementations.guards.emplace("canAccess", can_access);
    implementations.delays.emplace("shortDelay", [](const xstate::action_args&) {
        return xstate::result<std::uint64_t>(250);
    });

    const boost::json::value config = boost::json::parse(R"({
        "id": "access",
        "context": {"role": "admin"},
        "initial": "checking",
        "states": {
            "checking": {
                "after": {
                    "shortDelay": {
                        "guard": "canAccess",
                        "actions": "trackAccess",
                        "target": "allowed"
                    }
                }
            },
            "allowed": {}
        }
    })");
    const xstate::result<xstate::machine> access = xstate::create_machine(config, implementations);
    if (!access.has_value()) {
        return 1;
    }

    const auto [checking, entry_actions] = xstate::initial_transition(*access);
    for (const xstate::action& entry : entry_actions) {
        std::cout << entry.type << ' ' << boost::json::serialize(entry.params) << '\n';
    }

    const xstate::event delayed{.type = "xstate.after.shortDelay.access.checking", .payload = {}};
    const auto [allowed, actions] = xstate::transition(*access, checking, delayed);
    std::cout << boost::json::serialize(allowed.value);
    for (const xstate::action& taken : actions) {
        std::cout << ' ' << taken.type;
    }
    std::cout << '\n';
    // end::names[]
    return 0;
}
