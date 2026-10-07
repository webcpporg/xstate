// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string_view>

namespace xstate = webcpp::xstate;

int main() {
    // tag::definition[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "toggle",
        "initial": "inactive",
        "states": {
            "inactive": {"on": {"toggle": "active"}},
            "active": {"on": {"toggle": "inactive"}}
        }
    })");
    const xstate::result<xstate::machine> toggle = xstate::create_machine(config, {});
    if (!toggle.has_value()) {
        return 1;
    }

    const boost::json::value definition = xstate::to_json(*toggle);
    for (const std::string_view pointer : {
             "/id",
             "/order",
             "/initial/target",
             "/states/inactive/type",
             "/states/inactive/order",
             "/states/inactive/on/toggle/0/target",
             "/states/inactive/on/toggle/0/source",
         }) {
        boost::system::error_code missing;
        const boost::json::value* found = definition.find_pointer(pointer, missing);
        if (found == nullptr) {
            return 1;
        }
        std::cout << pointer << ' ' << boost::json::serialize(*found) << '\n';
    }
    // end::definition[]
    return 0;
}
