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
    const boost::json::value config = boost::json::parse(R"({
        "initial": "editing",
        "states": {
            "editing": {
                "on": {
                    "submit": {"guard": "isValid", "target": "submitted"}
                }
            },
            "submitted": {}
        }
    })");
    const xstate::result<xstate::machine> form = xstate::create_machine(config, {});
    if (!form.has_value()) {
        const boost::system::error_code& error = form.error();
        std::cout << error.category().name() << ": " << error.message() << '\n';
    }
    // end::refused[]
    return form.has_value() ? 1 : 0;
}
