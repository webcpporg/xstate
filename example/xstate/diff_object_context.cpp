// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <string>

namespace xstate = webcpp::xstate;

namespace {

// tag::helpers[]
/** {x: 1}, XState's assign({ x: 1 }). */
xstate::result<boost::json::object> add_x(const xstate::action_args& /*args*/) {
    return boost::json::object{{"x", 1}};
}

/** A context function that returns a string, () => 'ab'. */
xstate::result<boost::json::value> make_ab(const boost::json::value& /*input*/) {
    return boost::json::value("ab");
}

/** A snapshot's context, or its error when it failed. */
std::string describe(const xstate::snapshot& now) {
    if (now.status == xstate::status::error) {
        return "error, " + now.error.message();
    }
    return boost::json::serialize(now.context);
}

// end::helpers[]

}  // namespace

int main() {
    // tag::example[]
    xstate::implementations adder;
    adder.actions.emplace("add", xstate::assign_action{.assignment = add_x});
    const boost::json::value base = boost::json::parse(R"({
        "on": {"ADD": {"actions": "add"}}
    })");
    const boost::json::value contexts = boost::json::parse(R"([null, false, 5, "ab"])");
    for (const boost::json::value& context : contexts.get_array()) {
        boost::json::object config = base.get_object();
        config["context"] = context;
        const xstate::result<xstate::machine> machine = xstate::create_machine(config, adder);
        if (!machine.has_value()) {
            return 1;
        }
        const xstate::snapshot initial = xstate::get_initial_snapshot(*machine);
        const xstate::snapshot added =
            xstate::get_next_snapshot(*machine, initial, {.type = "ADD", .payload = {}});
        std::cout << "context " << boost::json::serialize(context) << ": " << describe(initial)
                  << ", after ADD " << describe(added) << '\n';
    }

    xstate::implementations maker;
    maker.context = make_ab;
    const xstate::result<xstate::machine> made =
        xstate::create_machine(boost::json::object{}, maker);
    if (!made.has_value()) {
        return 1;
    }
    std::cout << "a context function that returns \"ab\": "
              << describe(xstate::get_initial_snapshot(*made)) << '\n';
    // end::example[]
    return 0;
}
