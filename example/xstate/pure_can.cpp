// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string>
#include <string_view>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    const xstate::predicate has_text = [](const xstate::action_args& args) -> xstate::result<bool> {
        const boost::json::object* context = args.context.if_object();
        const boost::json::value* text =
            context == nullptr ? nullptr : context->if_contains("text");
        if (text == nullptr || !text->is_string()) {
            return xstate::failure<bool>(xstate::errc::implementation_failed);
        }
        return !text->get_string().empty();
    };
    const xstate::assigner keep =
        [](const xstate::action_args& args) -> xstate::result<boost::json::object> {
        const boost::json::value* text = args.event.payload.if_contains("text");
        if (text == nullptr) {
            return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"text", *text}};
    };
    xstate::implementations implementations;
    implementations.guards.emplace("hasText", has_text);
    implementations.actions.emplace("keep", xstate::assign_action{.assignment = keep});

    const boost::json::value config = boost::json::parse(R"({
        "id": "form",
        "initial": "editing",
        "context": {"text": ""},
        "states": {
            "editing": {
                "on": {
                    "SUBMIT": {"target": "submitted", "guard": "hasText"},
                    "TYPE": {"actions": "keep"},
                    "IGNORE": {}
                }
            },
            "submitted": {"type": "final"}
        }
    })");
    const xstate::result<xstate::machine> form = xstate::create_machine(config, implementations);
    if (!form.has_value()) {
        return 1;
    }

    const xstate::snapshot editing = xstate::get_initial_snapshot(*form);
    for (const std::string_view type : {"SUBMIT", "TYPE", "IGNORE", "UNKNOWN"}) {
        const xstate::result<bool> able =
            xstate::can(*form, editing, xstate::event{.type = std::string(type), .payload = {}});
        if (!able.has_value()) {
            return 1;
        }
        std::cout << type << ": " << (*able ? "true" : "false") << '\n';
    }

    const xstate::event type_hi{.type = "TYPE", .payload = {{"text", "hi"}}};
    const xstate::snapshot typed = xstate::get_next_snapshot(*form, editing, type_hi);
    const xstate::result<bool> submit =
        xstate::can(*form, typed, xstate::event{.type = "SUBMIT", .payload = {}});
    if (!submit.has_value()) {
        return 1;
    }
    std::cout << "after TYPE, SUBMIT: " << (*submit ? "true" : "false") << '\n';
    // end::example[]
    return 0;
}
