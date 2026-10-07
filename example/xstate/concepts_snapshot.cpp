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

std::string_view status_name(xstate::status of) {
    switch (of) {
        case xstate::status::active: return "active";
        case xstate::status::done: return "done";
        case xstate::status::error: return "error";
    }
    return "unknown";
}

}  // namespace

int main() {
    // tag::example[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "walk",
        "initial": "waiting",
        "context": {"dog": "Rex"},
        "states": {
            "waiting": {"on": {"leave home": "onAWalk"}},
            "onAWalk": {
                "tags": ["outside"],
                "initial": "walking",
                "states": {
                    "walking": {"on": {"speed up": "running"}},
                    "running": {}
                },
                "on": {"arrive home": "waiting"}
            }
        }
    })");
    const xstate::result<xstate::machine> walk = xstate::create_machine(config, {});
    if (!walk.has_value()) {
        return 1;
    }

    const xstate::snapshot waiting = xstate::get_initial_snapshot(*walk);
    const xstate::event leave_home{.type = "leave home", .payload = {}};
    const xstate::snapshot out = xstate::get_next_snapshot(*walk, waiting, leave_home);
    const xstate::result<bool> can_arrive =
        xstate::can(*walk, out, xstate::event{.type = "arrive home", .payload = {}});
    const xstate::result<bool> can_leave = xstate::can(*walk, out, leave_home);
    if (!can_arrive.has_value() || !can_leave.has_value()) {
        return 1;
    }

    std::cout << "value: " << boost::json::serialize(out.value) << '\n';
    std::cout << "context: " << boost::json::serialize(out.context) << '\n';
    std::cout << "status: " << status_name(out.status) << '\n';
    const boost::json::array tags(out.tags.begin(), out.tags.end());
    std::cout << "tags: " << boost::json::serialize(tags) << '\n';
    std::cout << std::boolalpha << "matches onAWalk: " << out.matches("onAWalk") << '\n';
    std::cout << "has tag outside: " << out.has_tag("outside") << '\n';
    std::cout << "can arrive home: " << *can_arrive << '\n';
    std::cout << "can leave home: " << *can_leave << '\n';
    // end::example[]
    return 0;
}
