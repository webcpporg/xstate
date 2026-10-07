// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstddef>
#include <iostream>
#include <string_view>

namespace xstate = webcpp::xstate;

namespace {

// tag::describe[]
std::string_view name_of(xstate::status status) {
    switch (status) {
        case xstate::status::active: return "active";
        case xstate::status::done: return "done";
        case xstate::status::error: return "error";
    }
    return "unknown";
}

/** Prints what a snapshot holds, naming its active nodes by their ids. */
void describe(const xstate::machine& owner, const xstate::snapshot& now) {
    boost::json::array nodes;
    for (const std::size_t node : now.nodes) {
        nodes.emplace_back(owner.node(node).id);
    }
    const boost::json::array tags(now.tags.begin(), now.tags.end());
    std::cout << "value: " << boost::json::serialize(now.value) << '\n';
    std::cout << "context: " << boost::json::serialize(now.context) << '\n';
    std::cout << "status: " << name_of(now.status) << '\n';
    std::cout << "nodes: " << boost::json::serialize(nodes) << '\n';
    std::cout << "tags: " << boost::json::serialize(tags) << '\n';
    if (now.status == xstate::status::done && now.output.has_value()) {
        std::cout << "output: " << boost::json::serialize(*now.output) << '\n';
    }
}

// end::describe[]

}  // namespace

int main() {
    // tag::run[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "context": {"feedback": ""},
        "initial": "form",
        "states": {
            "form": {
                "tags": ["visible"],
                "initial": "invalid",
                "states": {
                    "invalid": {"on": {"feedback.valid": "valid"}},
                    "valid": {}
                },
                "on": {"feedback.submit": "thanks"}
            },
            "thanks": {"type": "final"}
        },
        "output": {"sent": true}
    })");
    const xstate::result<xstate::machine> feedback = xstate::create_machine(config, {});
    if (!feedback.has_value()) {
        return 1;
    }

    const xstate::snapshot first = xstate::get_initial_snapshot(*feedback);
    describe(*feedback, first);

    const xstate::event submit{.type = "feedback.submit", .payload = {}};
    describe(*feedback, xstate::get_next_snapshot(*feedback, first, submit));
    // end::run[]
    return 0;
}
