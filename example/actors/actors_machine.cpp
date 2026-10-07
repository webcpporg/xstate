// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>
#include <string_view>

namespace xstate = webcpp::xstate;
namespace xactor = webcpp::xactor;

namespace {

std::string_view name_of(xactor::status status) {
    switch (status) {
        case xactor::status::active: return "active";
        case xactor::status::done: return "done";
        case xactor::status::error: return "error";
        case xactor::status::stopped: return "stopped";
    }
    return "unknown";
}

}  // namespace

int main() {
    // tag::machine[]
    xstate::implementations implementations;
    implementations.context =
        [](const boost::json::value& input) -> xstate::result<boost::json::value> {
        const boost::json::object* given = input.if_object();
        const boost::json::value* rating =
            given == nullptr ? nullptr : given->if_contains("defaultRating");
        if (rating == nullptr) {
            return xstate::failure<boost::json::value>(xstate::errc::implementation_failed);
        }
        return boost::json::object{{"rating", *rating}};
    };
    // By the root's id: the machine's output.
    implementations.outputs.emplace(
        "feedback",
        [](const xstate::action_args& args) -> xstate::result<std::optional<boost::json::value>> {
            return std::optional<boost::json::value>(args.context);
        });
    const boost::json::value config = boost::json::parse(R"({
        "id": "feedback",
        "initial": "question",
        "states": {
            "question": {"on": {"submit": "thanks"}},
            "thanks": {"type": "final"}
        }
    })");
    const xstate::result<xstate::machine> feedback_machine =
        xstate::create_machine(config, implementations);
    if (!feedback_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::actor_options options{
        .input = boost::json::object{{"defaultRating", 3}},
        .id = std::nullopt,
        .system_id = std::nullopt,
    };
    const xstate::result<xstate::actor_ref> feedback_actor =
        system.create_actor(*feedback_machine, options);
    if (!feedback_actor.has_value()) {
        return 1;
    }
    const xstate::result<void> subscribed =
        system.subscribe(*feedback_actor, [](const xstate::snapshot& snapshot) {
            std::cout << boost::json::serialize(snapshot.context) << '\n';
        });
    if (!subscribed.has_value()) {
        return 1;
    }
    if (!system.start(*feedback_actor).has_value()) {
        return 1;
    }
    // logs {"rating":3}
    if (!system.send(*feedback_actor, {.type = "submit", .payload = {}}).has_value()) {
        return 1;
    }
    // logs {"rating":3}, the snapshot it is done in

    const xstate::result<xactor::status> status = system.status_of(*feedback_actor);
    if (!status.has_value()) {
        return 1;
    }
    const xstate::snapshot* last = system.snapshot_of(*feedback_actor);
    if (last == nullptr) {
        return 1;
    }
    if (!last->output.has_value()) {
        return 1;
    }
    std::cout << "status: " << name_of(*status) << '\n'
              << "output: " << boost::json::serialize(*last->output) << '\n';
    // end::machine[]
    return 0;
}
