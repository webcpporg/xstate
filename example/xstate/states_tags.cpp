// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

namespace {

// tag::show[]
/** Prints a snapshot's value, its tags, and whether one of them is "visible". */
void show(const xstate::snapshot& now) {
    const boost::json::array tags(now.tags.begin(), now.tags.end());
    std::cout << boost::json::serialize(now.value) << ' ' << tags << " visible: " << std::boolalpha
              << now.has_tag("visible") << '\n';
}

// end::show[]

}  // namespace

int main() {
    // tag::tags[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "prompt",
        "states": {
            "prompt": {"tags": ["visible"], "on": {"feedback.good": "thanks"}},
            "thanks": {
                "tags": ["visible"],
                "initial": "celebrating",
                "states": {"celebrating": {"tags": ["visible", "confetti"]}},
                "on": {"feedback.close": "closed"}
            },
            "closed": {"tags": "hidden"}
        }
    })");
    const xstate::result<xstate::machine> feedback = xstate::create_machine(config, {});
    if (!feedback.has_value()) {
        return 1;
    }
    xstate::snapshot now = xstate::get_initial_snapshot(*feedback);
    show(now);

    const xstate::event good{.type = "feedback.good", .payload = {}};
    now = xstate::get_next_snapshot(*feedback, now, good);
    show(now);

    const xstate::event close{.type = "feedback.close", .payload = {}};
    now = xstate::get_next_snapshot(*feedback, now, close);
    show(now);
    // end::tags[]
    return 0;
}
