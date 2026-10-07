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
#include <string_view>
#include <vector>

namespace xstate = webcpp::xstate;

namespace {

/** The name of a status, as XState spells it. */
std::string_view name_of(xstate::status status) {
    switch (status) {
        case xstate::status::active: return "active";
        case xstate::status::done: return "done";
        case xstate::status::error: return "error";
    }
    return "unknown";
}

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

// tag::execute[]
/**
 Runs what XState's actor runs as it resolves an action: a custom action,
 and a log, which it hands to its logger.

 Tip: XState's actor holds what an emit, the other built-in action here,
 does until the macrostep ends, which a macrostep that fails never reaches.
*/
void execute(const xstate::action& action) {
    if (!action.builtin) {
        std::cout << "ran " << action.type << '\n';
        return;
    }
    if (action.type != "xstate.log" || !action.params.is_object()) {
        return;
    }
    const boost::json::value* value = action.params.get_object().if_contains("value");
    if (value == nullptr || !value->is_string()) {
        return;
    }
    const std::string_view text = value->get_string();
    std::cout << "logged: " << text << '\n';
}

// end::execute[]

}  // namespace

int main() {
    // tag::example[]
    const xstate::event_maker told =
        [](const xstate::action_args&) -> xstate::result<xstate::event> {
        return xstate::event{.type = "told", .payload = {}};
    };
    const xstate::value_maker said =
        [](const xstate::action_args&) -> xstate::result<std::optional<boost::json::value>> {
        return std::optional<boost::json::value>("say");
    };
    const xstate::predicate broken = [](const xstate::action_args&) -> xstate::result<bool> {
        return xstate::failure<bool>(xstate::errc::implementation_failed);
    };
    xstate::implementations implementations;
    implementations.actions.emplace("tell", xstate::emit_action{.event = told});
    implementations.actions.emplace("say",
                                    xstate::log_action{.value = said, .label = std::nullopt});
    implementations.guards.emplace("broken", broken);

    const boost::json::value config = boost::json::parse(R"({
        "id": "order",
        "initial": "idle",
        "states": {
            "idle": {"on": {"GO": {"target": "checking", "actions": ["note", "tell", "say"]}}},
            "checking": {"always": {"target": "done", "guard": "broken"}},
            "done": {}
        }
    })");
    const xstate::result<xstate::machine> order = xstate::create_machine(config, implementations);
    if (!order.has_value()) {
        return 1;
    }
    const xstate::snapshot idle = xstate::get_initial_snapshot(*order);

    xstate::macrostep cursor =
        xstate::begin(*order, idle, xstate::event{.type = "GO", .payload = {}});
    while (!cursor.done()) {
        const xstate::progress made = cursor.next();
        std::cout << "step: " << types_of(made.step.actions) << (made.settled ? ", settled" : "")
                  << '\n';
        for (const xstate::action& action : made.step.actions) {
            execute(action);
        }
        for (const xstate::action& action : made.resolved_before_failure) {
            execute(action);
        }
    }
    const xstate::snapshot& settled = cursor.result().snapshot;
    std::cout << "status: " << name_of(settled.status) << " (" << settled.error.message() << ")\n";
    std::cout << "value: " << boost::json::serialize(settled.value) << '\n';
    // end::example[]
    return 0;
}
