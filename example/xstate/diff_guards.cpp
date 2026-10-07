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
    // tag::example[]
    xstate::implementations implementations;
    implementations.guards.emplace(
        "isEmpty", [](const xstate::action_args& args) -> xstate::result<bool> {
            const boost::json::value* name = args.event.payload.if_contains("name");
            if (name == nullptr) {
                return true;
            }
            return name->is_string() && name->get_string().empty();
        });
    const boost::json::value config = boost::json::parse(R"({
        "id": "form",
        "initial": "editing",
        "states": {
            "editing": {
                "on": {
                    "SUBMIT": {
                        "target": "submitted",
                        "guard": {"type": "xstate.not", "guards": ["isEmpty"]}
                    }
                }
            },
            "submitted": {}
        }
    })");
    const xstate::result<xstate::machine> form = xstate::create_machine(config, implementations);
    if (!form.has_value()) {
        return 1;
    }
    const xstate::snapshot editing = xstate::get_initial_snapshot(*form);
    const xstate::snapshot unnamed = xstate::get_next_snapshot(
        *form, editing, xstate::event{.type = "SUBMIT", .payload = {{"name", ""}}});
    const xstate::snapshot named = xstate::get_next_snapshot(
        *form, editing, xstate::event{.type = "SUBMIT", .payload = {{"name", "Ana"}}});
    std::cout << "SUBMIT without a name: " << boost::json::serialize(unnamed.value) << '\n'
              << "SUBMIT with a name: " << boost::json::serialize(named.value) << '\n';

    const boost::json::value definition = xstate::to_json(*form);
    boost::system::error_code missing;
    const boost::json::value* guard =
        definition.find_pointer("/states/editing/on/SUBMIT/0/guard", missing);
    std::cout << "the definition's guard: "
              << (guard == nullptr ? "undefined" : boost::json::serialize(*guard)) << '\n';
    // end::example[]
    return 0;
}
