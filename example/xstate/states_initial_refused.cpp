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
    // tag::refused[]
    const boost::json::value without_initial = boost::json::parse(R"({
        "initial": "red",
        "states": {
            "red": {"states": {"walk": {}, "wait": {}}}
        }
    })");
    const xstate::result<xstate::machine> first = xstate::create_machine(without_initial, {});
    if (first.has_value()) {
        return 1;
    }
    std::cout << first.error().message() << '\n';

    const boost::json::value unknown_initial = boost::json::parse(R"({
        "initial": "nowhere",
        "states": {"red": {}}
    })");
    const xstate::result<xstate::machine> second = xstate::create_machine(unknown_initial, {});
    if (second.has_value()) {
        return 1;
    }
    std::cout << second.error().message() << '\n';
    // end::refused[]
    return 0;
}
