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
/** The symptom event, its symptom taken from the action's params. */
xstate::result<xstate::event> symptom_event(const xstate::action_args& args) {
    const boost::json::object* params = args.params.if_object();
    const boost::json::value* symptom =
        params == nullptr ? nullptr : params->if_contains("symptom");
    if (symptom == nullptr) {
        return xstate::failure<xstate::event>(xstate::errc::implementation_failed);
    }
    return xstate::event{.type = "symptom", .payload = {{"symptom", *symptom}}};
}

/** Keeps the symptom of the event being handled. */
xstate::result<boost::json::object> note(const xstate::action_args& args) {
    const boost::json::value* symptom = args.event.payload.if_contains("symptom");
    if (symptom == nullptr) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"last", *symptom}};
}

// end::implementations[]

/** Whether a call that delivers ran to its end: no error, and no actor left parked. */
bool settled(const xstate::result<xstate::run_outcome>& ran) {
    return ran.has_value() && *ran == xstate::run_outcome::settled;
}

}  // namespace

int main() {
    // tag::example[]
    xstate::implementations vet_implementations;
    vet_implementations.actions.emplace("note", xstate::assign_action{.assignment = note});
    const boost::json::value vet_config = boost::json::parse(R"({
        "id": "vet",
        "context": {"last": null},
        "on": {"symptom": {"actions": "note"}}
    })");
    const xstate::result<xstate::machine> vet =
        xstate::create_machine(vet_config, vet_implementations);
    if (!vet.has_value()) {
        return 1;
    }

    const xstate::send_to_action call_vet{
        .target = "#system:vet",
        .event = symptom_event,
        .id = std::nullopt,
        .delay = std::nullopt,
    };
    xstate::implementations walker_implementations;
    walker_implementations.actions.emplace("callVet", call_vet);
    const boost::json::value walker_config = boost::json::parse(R"({
        "id": "walker",
        "on": {
            "sees a limp": {"actions": {"type": "callVet", "params": {"symptom": "limps"}}}
        }
    })");
    const xstate::result<xstate::machine> walker =
        xstate::create_machine(walker_config, walker_implementations);
    if (!walker.has_value()) {
        return 1;
    }

    xstate::implementations owner_implementations;
    owner_implementations.actors.emplace("vet", xstate::machine_actor{*vet});
    owner_implementations.actors.emplace("walker", xstate::machine_actor{*walker});
    const boost::json::value owner_config = boost::json::parse(R"({
        "id": "owner",
        "invoke": [
            {"id": "vet", "src": "vet", "systemId": "vet"},
            {"id": "walker", "src": "walker"}
        ]
    })");
    const xstate::result<xstate::machine> owner =
        xstate::create_machine(owner_config, owner_implementations);
    if (!owner.has_value()) {
        return 1;
    }

    xstate::actor_system system({.fuel = 10'000});
    const xstate::result<xstate::actor_ref> owner_actor = system.create_actor(*owner);
    if (!owner_actor.has_value() || !settled(system.start(*owner_actor))) {
        return 1;
    }
    const std::optional<xstate::actor_ref> walker_actor = system.child_of(*owner_actor, "walker");
    if (!walker_actor.has_value() ||
        !settled(system.send(*walker_actor, {.type = "sees a limp", .payload = {}}))) {
        return 1;
    }
    const std::optional<xstate::actor_ref> vet_actor = system.get("vet");
    if (!vet_actor.has_value()) {
        return 1;
    }
    const xstate::snapshot* heard = system.snapshot_of(*vet_actor);
    if (heard == nullptr) {
        return 1;
    }
    std::cout << "vet: " << boost::json::serialize(heard->context) << '\n';
    // end::example[]
    return 0;
}
