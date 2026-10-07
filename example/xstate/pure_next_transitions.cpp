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

namespace xstate = webcpp::xstate;

namespace {

// tag::targets[]
/** The ids of a transition's targets, or "(targetless)". */
std::string targets_of(const xstate::machine& owner, const xstate::transition_definition& one) {
    if (!one.target.has_value()) {
        return "(targetless)";
    }
    std::string listed;
    for (const std::size_t node : *one.target) {
        if (!listed.empty()) {
            listed += ", ";
        }
        listed += owner.node(node).id;
    }
    return listed;
}

// end::targets[]

}  // namespace

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "form",
        "initial": "editing",
        "on": {"RESET": ".editing"},
        "states": {
            "editing": {
                "on": {
                    "SUBMIT": {"target": "submitted", "guard": "hasText"},
                    "TYPE": {"actions": "keep"}
                },
                "always": {"target": "submitted", "guard": "isFull"}
            },
            "submitted": {"type": "final"}
        }
    })");
    xstate::implementations implementations;
    implementations.guards.emplace("hasText", [](const xstate::action_args&) { return true; });
    implementations.guards.emplace("isFull", [](const xstate::action_args&) { return false; });
    const xstate::result<xstate::machine> form = xstate::create_machine(config, implementations);
    if (!form.has_value()) {
        return 1;
    }

    const xstate::snapshot editing = xstate::get_initial_snapshot(*form);
    for (const xstate::transition_definition* one : xstate::get_next_transitions(*form, editing)) {
        std::cout << '"' << one->event_type << "\" " << form->node(one->source).id << " -> "
                  << targets_of(*form, *one) << '\n';
    }
    // end::example[]
    return 0;
}
