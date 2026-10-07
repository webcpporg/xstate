// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <iostream>
#include <optional>

namespace xstate = webcpp::xstate;

namespace {

// tag::implementations[]
xstate::result<xstate::event> notification(const xstate::action_args& /*args*/) {
    return xstate::event{.type = "notify", .payload = {{"message", "Form submitted!"}}};
}

/** {last: event.message} */
xstate::result<boost::json::object> keep_last(const xstate::action_args& args) {
    const boost::json::value* message = args.event.payload.if_contains("message");
    if (message == nullptr) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"last", *message}};
}

// end::implementations[]

}  // namespace

int main() {
    // tag::system[]
    xstate::implementations notifier_implementations;
    notifier_implementations.actions.emplace("keep",
                                             xstate::assign_action{.assignment = keep_last});
    const boost::json::value notifier_config = boost::json::parse(R"({
        "context": {"last": null},
        "on": {"notify": {"actions": "keep"}}
    })");
    const xstate::result<xstate::machine> notifier_machine =
        xstate::create_machine(notifier_config, notifier_implementations);
    if (!notifier_machine.has_value()) {
        return 1;
    }

    xstate::implementations form_implementations;
    const xstate::send_to_action notify{
        .target = "#system:notifier",
        .event = notification,
        .id = std::nullopt,
        .delay = std::nullopt,
    };
    form_implementations.actions.emplace("notify", notify);
    const boost::json::value form_config = boost::json::parse(R"({
        "on": {"submit": {"actions": "notify"}}
    })");
    const xstate::result<xstate::machine> form_machine =
        xstate::create_machine(form_config, form_implementations);
    if (!form_machine.has_value()) {
        return 1;
    }

    xstate::implementations feedback_implementations;
    feedback_implementations.actors.emplace("formMachine", xstate::machine_actor{*form_machine});
    feedback_implementations.actors.emplace("notifierMachine",
                                            xstate::machine_actor{*notifier_machine});
    const boost::json::value feedback_config = boost::json::parse(R"({
        "invoke": [
            {"systemId": "formMachine", "src": "formMachine"},
            {"systemId": "notifier", "src": "notifierMachine"}
        ]
    })");
    const xstate::result<xstate::machine> feedback_machine =
        xstate::create_machine(feedback_config, feedback_implementations);
    if (!feedback_machine.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> feedback_actor = system.create_actor(*feedback_machine);
    if (!feedback_actor.has_value()) {
        return 1;
    }
    if (!system.start(*feedback_actor).has_value()) {
        return 1;
    }
    const std::optional<xstate::actor_ref> form = system.get("formMachine");
    if (!form.has_value()) {
        return 1;
    }
    if (!system.send(*form, {.type = "submit", .payload = {}}).has_value()) {
        return 1;
    }
    const std::optional<xstate::actor_ref> notifier = system.get("notifier");
    if (!notifier.has_value()) {
        return 1;
    }
    const xstate::snapshot* notified = system.snapshot_of(*notifier);
    if (notified == nullptr) {
        return 1;
    }
    std::cout << boost::json::serialize(notified->context) << '\n';
    // logs {"last":"Form submitted!"}
    // end::system[]
    return 0;
}
