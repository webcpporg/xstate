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
    const boost::json::value config = boost::json::parse(R"({
        "id": "dog",
        "initial": "asleep",
        "states": {
            "asleep": {},
            "awake": {}
        }
    })");
    const xstate::result<xstate::machine> dog = xstate::create_machine(config, {});
    if (!dog.has_value()) {
        return 1;
    }

    const xstate::snapshot first = xstate::get_initial_snapshot(*dog);
    std::cout << "value: " << boost::json::serialize(first.value) << '\n';
    std::cout << std::boolalpha << "asleep: " << first.matches("asleep") << '\n';
    std::cout << "awake: " << first.matches("awake") << '\n';
    // end::example[]
    return 0;
}
