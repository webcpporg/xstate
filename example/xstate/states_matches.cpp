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
    // tag::matches[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "question",
        "states": {
            "question": {"on": {"feedback.bad": "form"}},
            "form": {"initial": "invalid", "states": {"invalid": {}, "valid": {}}}
        }
    })");
    const xstate::result<xstate::machine> feedback = xstate::create_machine(config, {});
    if (!feedback.has_value()) {
        return 1;
    }
    const xstate::event bad{.type = "feedback.bad", .payload = {}};
    const xstate::snapshot now =
        xstate::get_next_snapshot(*feedback, xstate::get_initial_snapshot(*feedback), bad);
    std::cout << "value: " << boost::json::serialize(now.value) << '\n';

    const boost::json::value tested = boost::json::parse(R"([
        "question",
        "form",
        {"form": "invalid"},
        {"form": "valid"},
        "form.invalid"
    ])");
    std::cout << std::boolalpha;
    for (const boost::json::value& value : tested.get_array()) {
        std::cout << boost::json::serialize(value) << ": " << now.matches(value) << '\n';
    }

    const boost::json::value playing =
        boost::json::parse(R"({"track": "playing", "volume": "normal"})");
    const boost::json::value track_playing = boost::json::parse(R"({"track": "playing"})");
    std::cout << boost::json::serialize(track_playing) << " in " << boost::json::serialize(playing)
              << ": " << xstate::matches_state(track_playing, playing) << '\n';
    // end::matches[]
    return 0;
}
