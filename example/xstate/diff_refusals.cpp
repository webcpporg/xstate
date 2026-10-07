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

// tag::report[]
/** Prints whether create_machine accepts `config`, and the error when it refuses it. */
void report(std::string_view label, const boost::json::value& config) {
    const xstate::result<xstate::machine> created = xstate::create_machine(config, {});
    if (!created.has_value()) {
        std::cout << label << ": refused, " << created.error().message() << '\n';
        return;
    }
    std::cout << label << ": created\n";
}

// end::report[]

}  // namespace

int main() {
    // tag::configs[]
    const boost::json::value no_such_initial = boost::json::parse(R"({
        "id": "m",
        "initial": "nowhere",
        "states": {"a": {}}
    })");
    const boost::json::value unknown_guard = boost::json::parse(R"({
        "id": "m",
        "initial": "a",
        "states": {
            "a": {"on": {"GO": {"target": "b", "guard": "isReady"}}},
            "b": {}
        }
    })");
    const boost::json::value unknown_delay = boost::json::parse(R"({
        "id": "m",
        "initial": "a",
        "states": {
            "a": {"after": {"slow": "b"}},
            "b": {}
        }
    })");
    const boost::json::value endless_entry = boost::json::parse(R"({
        "id": "m",
        "initial": "a",
        "states": {
            "a": {
                "initial": "h",
                "states": {
                    "h": {"type": "history", "target": "#m.a"},
                    "x": {}
                }
            }
        }
    })");
    const boost::json::value null_exit = boost::json::parse(R"({
        "id": "m",
        "initial": "a",
        "states": {
            "a": {"exit": null, "on": {"GO": "b"}},
            "b": {}
        }
    })");
    const boost::json::value unknown_actor = boost::json::parse(R"({
        "id": "m",
        "invoke": {"id": "w", "src": "worker"}
    })");
    report("an initial that names no child", no_such_initial);
    report("a guard with no implementation", unknown_guard);
    report("a delay with no implementation", unknown_delay);
    report("a default entry that leads back", endless_entry);
    report("an exit that is null", null_exit);
    report("an invoke of an unknown actor", unknown_actor);
    // end::configs[]
    return 0;
}
