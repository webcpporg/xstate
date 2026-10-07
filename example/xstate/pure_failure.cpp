// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string_view>

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

// tag::execute[]
/** Executes a custom action, the only kind this machine returns for its caller to run. */
void execute(const xstate::action& action) {
    if (action.builtin) {
        return;
    }
    std::cout << "ran " << action.type << '\n';
}

// end::execute[]

}  // namespace

int main() {
    // tag::example[]
    const xstate::assigner count_attempt =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* attempts =
            context == nullptr ? nullptr : context->if_contains("attempts");
        if (attempts == nullptr || !attempts->is_int64()) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"attempts", attempts->get_int64() + 1}};
    };
    const xstate::assigner fail =
        [](const xstate::action_args&) -> xstate::result<boost::json::object> {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    };
    xstate::implementations implementations;
    implementations.actions.emplace("countAttempt",
                                    xstate::assign_action{.assignment = count_attempt});
    implementations.actions.emplace("fail", xstate::assign_action{.assignment = fail});

    const boost::json::value config = boost::json::parse(R"({
        "id": "upload",
        "initial": "idle",
        "context": {"attempts": 0},
        "states": {
            "idle": {"on": {"UPLOAD": {"target": "uploading", "actions": "notify"}}},
            "uploading": {
                "always": {
                    "target": "done",
                    "actions": ["progress", "countAttempt", "fail"]
                }
            },
            "done": {}
        }
    })");
    const xstate::result<xstate::machine> upload = xstate::create_machine(config, implementations);
    if (!upload.has_value()) {
        return 1;
    }
    const xstate::snapshot idle = xstate::get_initial_snapshot(*upload);

    xstate::macrostep cursor =
        xstate::begin(*upload, idle, xstate::event{.type = "UPLOAD", .payload = {}});
    while (!cursor.done()) {
        const xstate::progress made = cursor.next();
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
    std::cout << "context: " << boost::json::serialize(settled.context) << '\n';
    // end::example[]
    return 0;
}
