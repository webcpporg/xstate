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
    // tag::parent[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "machine",
        "initial": "parent",
        "states": {
            "parent": {
                "initial": "child1",
                "states": {
                    "child1": {"on": {"next": "child2"}},
                    "child2": {
                        "initial": "grandchild1",
                        "states": {"grandchild1": {}, "grandchild2": {}}
                    }
                },
                "on": {
                    "next": ".child2.grandchild2",
                    "restart": ".child1"
                }
            }
        }
    })");
    const xstate::result<xstate::machine> nested = xstate::create_machine(config, {});
    if (!nested.has_value()) {
        return 1;
    }
    xstate::snapshot now = xstate::get_initial_snapshot(*nested);
    std::cout << boost::json::serialize(now.value) << '\n';

    const xstate::event next{.type = "next", .payload = {}};
    now = xstate::get_next_snapshot(*nested, now, next);  // child1's own transition
    std::cout << boost::json::serialize(now.value) << '\n';

    now = xstate::get_next_snapshot(*nested, now, next);  // the parent's transition
    std::cout << boost::json::serialize(now.value) << '\n';

    const xstate::event restart{.type = "restart", .payload = {}};
    now = xstate::get_next_snapshot(*nested, now, restart);
    std::cout << boost::json::serialize(now.value) << '\n';
    // end::parent[]
    return 0;
}
