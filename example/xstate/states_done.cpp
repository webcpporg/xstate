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

namespace {

std::string_view name_of(xstate::status status) {
    switch (status) {
        case xstate::status::active: return "active";
        case xstate::status::done: return "done";
        case xstate::status::error: return "error";
    }
    return "unknown";
}

}  // namespace

int main() {
    // tag::done[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "process",
        "initial": "running",
        "states": {
            "running": {"on": {"finish": "complete"}},
            "complete": {"type": "final"}
        },
        "output": {"message": "Process completed."}
    })");
    const xstate::result<xstate::machine> process = xstate::create_machine(config, {});
    if (!process.has_value()) {
        return 1;
    }
    xstate::snapshot now = xstate::get_initial_snapshot(*process);
    std::cout << boost::json::serialize(now.value) << ' ' << name_of(now.status) << '\n';

    now = xstate::get_next_snapshot(*process, now, xstate::event{.type = "finish", .payload = {}});
    std::cout << boost::json::serialize(now.value) << ' ' << name_of(now.status) << '\n';
    if (now.status == xstate::status::done && now.output.has_value()) {
        std::cout << "output: " << boost::json::serialize(*now.output) << '\n';
    }
    // end::done[]
    return 0;
}
