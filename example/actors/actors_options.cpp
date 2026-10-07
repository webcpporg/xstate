// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>

namespace xstate = webcpp::xstate;

int main() {
    const boost::json::value config = boost::json::parse("{}");
    const xstate::result<xstate::machine> machine = xstate::create_machine(config, {});
    if (!machine.has_value()) {
        return 1;
    }

    // tag::options[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::actor_options options{
        .input = nullptr,
        .id = "feedback",
        .system_id = "root-id",
    };
    const xstate::result<xstate::actor_ref> named = system.create_actor(*machine, options);
    if (!named.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> unnamed = system.create_actor(*machine);
    if (!unnamed.has_value()) {
        return 1;
    }
    std::cout << system.id_of(*named) << '\n';    // feedback
    std::cout << system.id_of(*unnamed) << '\n';  // x:1

    if (!system.start(*named).has_value()) {
        return 1;
    }
    const std::optional<xstate::actor_ref> found = system.get("root-id");
    std::cout << std::boolalpha << (found == *named) << '\n';  // true
    // end::options[]
    return 0;
}
