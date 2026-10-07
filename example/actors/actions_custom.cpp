// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

namespace {

// tag::execute[]
/** Runs a custom action the machine returned; one it does not know does nothing. */
void execute(const xstate::action& custom) {
    if (custom.type == "track") {
        const boost::json::object* params = custom.params.if_object();
        const boost::json::value* response =
            params == nullptr ? nullptr : params->if_contains("response");
        std::cout << "tracking " << (response == nullptr ? "-" : boost::json::serialize(*response))
                  << '\n';
        return;
    }
    if (custom.type == "showConfetti") {
        std::cout << "confetti!\n";
    }
}

// end::execute[]

}  // namespace

int main() {
    // tag::machine[]
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "question",
        "states": {
            "question": {
                "on": {
                    "feedback.good": {
                        "target": "thanks",
                        "actions": [
                            {"type": "track", "params": {"response": "good"}}
                        ]
                    }
                },
                "exit": [{"type": "exitAction"}]
            },
            "thanks": {"entry": [{"type": "showConfetti"}]}
        }
    })");
    const xstate::result<xstate::machine> feedback = xstate::create_machine(config, {});
    if (!feedback.has_value()) {
        return 1;
    }
    const xstate::event good{.type = "feedback.good", .payload = {}};
    // end::machine[]

    // tag::pure[]
    const xstate::snapshot question = xstate::get_initial_snapshot(*feedback);
    const auto [thanks, actions] = xstate::transition(*feedback, question, good);
    std::cout << "returned:";
    for (const xstate::action& one : actions) {
        std::cout << ' ' << one.type;
    }
    std::cout << '\n';
    for (const xstate::action& one : actions) {
        execute(one);
    }
    // end::pure[]

    // tag::actor[]
    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<void> acting =
        system.on_action([](xstate::actor_ref, const xstate::action& custom) { execute(custom); });
    if (!acting.has_value()) {
        return 1;
    }
    const xstate::result<xstate::actor_ref> actor = system.create_actor(*feedback);
    if (!actor.has_value() || !system.start(*actor).has_value()) {
        return 1;
    }
    std::cout << "the actor:\n";
    if (!system.send(*actor, good).has_value()) {
        return 1;
    }
    // end::actor[]
    return 0;
}
