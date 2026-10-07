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

namespace xstate = webcpp::xstate;

namespace {

/** What creating a machine says: "created", or why it refused. */
std::string created(const boost::json::value& config, const xstate::implementations& registry) {
    const xstate::result<xstate::machine> made = xstate::create_machine(config, registry);
    if (!made.has_value()) {
        return made.error().message();
    }
    return "created";
}

}  // namespace

int main() {
    // tag::refused[]
    const xstate::value_maker nothing =
        [](const xstate::action_args&) -> xstate::result<std::optional<boost::json::value>> {
        return std::optional<boost::json::value>();
    };
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "loading",
        "states": {
            "loading": {
                "invoke": {
                    "id": "fetchUser",
                    "src": "fetchUser",
                    "input": {"userId": "42"}
                },
                "on": {"loaded": "closed", "cancel": "cancelled"}
            },
            "closed": {"type": "final"},
            "cancelled": {"type": "final", "output": "cancelled"}
        }
    })");
    xstate::implementations registry;
    registry.actors.emplace("fetchUser", xstate::host_actor{});

    xstate::implementations no_such_invoke = registry;
    no_such_invoke.inputs.emplace("fetchUsers", nothing);
    xstate::implementations input_twice = registry;
    input_twice.inputs.emplace("fetchUser", nothing);
    xstate::implementations not_final = registry;
    not_final.outputs.emplace("feedback.loading", nothing);
    xstate::implementations output_twice = registry;
    output_twice.outputs.emplace("feedback.cancelled", nothing);
    xstate::implementations final_and_root = registry;
    final_and_root.outputs.emplace("feedback.closed", nothing);
    final_and_root.outputs.emplace("feedback", nothing);

    std::cout << "an input for no invoke: " << created(config, no_such_invoke) << '\n'
              << "an input the config fixes too: " << created(config, input_twice) << '\n'
              << "an output for a state not final: " << created(config, not_final) << '\n'
              << "an output the config fixes too: " << created(config, output_twice) << '\n'
              << "outputs for a final state and the root: " << created(config, final_and_root)
              << '\n';
    // end::refused[]
    return 0;
}
