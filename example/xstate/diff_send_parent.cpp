// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    xstate::implementations implementations;
    implementations.actions.emplace(
        "reportDone", xstate::send_parent_action{
                          .event = [](const xstate::action_args&) -> xstate::result<xstate::event> {
                              return xstate::event{.type = "DONE", .payload = {}};
                          },
                          .id = std::nullopt,
                          .delay = std::nullopt,
                      });
    const boost::json::value config = boost::json::parse(R"({
        "id": "child",
        "on": {"FINISH": {"actions": "reportDone"}}
    })");
    const xstate::result<xstate::machine> child = xstate::create_machine(config, implementations);
    if (!child.has_value()) {
        return 1;
    }
    const auto [next, actions] = xstate::transition(*child, xstate::get_initial_snapshot(*child),
                                                    xstate::event{.type = "FINISH", .payload = {}});
    for (const xstate::action& returned : actions) {
        std::cout << returned.type << ' ' << boost::json::serialize(returned.params) << '\n';
    }
    // end::example[]
    return 0;
}
