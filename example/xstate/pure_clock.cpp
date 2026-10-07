// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace xstate = webcpp::xstate;

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "light",
        "initial": "green",
        "states": {
            "green": {"after": {"1000": "yellow"}},
            "yellow": {"after": {"500": "red"}, "on": {"STOP": "red"}},
            "red": {}
        }
    })");
    const xstate::result<xstate::machine> light = xstate::create_machine(config, {});
    if (!light.has_value()) {
        return 1;
    }

    xstate::simulated_clock clock;
    auto [now, initial_actions] = xstate::initial_transition(*light);
    for (const xstate::action& returned : initial_actions) {
        clock.apply(returned);
    }

    const auto deliver = [&](const xstate::event& happened) {
        auto [next, actions] = xstate::transition(*light, now, happened);
        for (const xstate::action& returned : actions) {
            clock.apply(returned);
        }
        now = std::move(next);
        std::cout << "  " << happened.type << " -> " << boost::json::serialize(now.value) << '\n';
    };

    const std::vector<std::uint64_t> times{500, 1000, 1200, 2000};
    for (const std::uint64_t time : times) {
        if (!clock.set(time).has_value()) {
            return 1;
        }
        std::cout << "at " << time << ":\n";
        if (time == 1200) {
            deliver(xstate::event{.type = "STOP", .payload = {}});
        }
        while (const std::optional<xstate::event> due = clock.pop_due()) {
            deliver(*due);
        }
    }
    // end::example[]
    return 0;
}
