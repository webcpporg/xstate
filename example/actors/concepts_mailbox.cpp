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
/** One more fetch, merged into the dog's context. */
xstate::result<boost::json::object> count(const xstate::action_args& args) {
    const boost::json::object* context = args.context.if_object();
    const boost::json::value* fetched =
        context == nullptr ? nullptr : context->if_contains("fetched");
    if (fetched == nullptr || !fetched->is_int64()) {
        return xstate::failure<boost::json::object>(xstate::errc::implementation_failed);
    }
    return boost::json::object{{"fetched", fetched->get_int64() + 1}};
}

/** The event the owner sends its dog. */
xstate::result<xstate::event> fetch(const xstate::action_args& /*args*/) {
    return xstate::event{.type = "fetch", .payload = {}};
}

// end::implementations[]

/** Whether a call that delivers ran to its end: no error, and no actor left parked. */
bool settled(const xstate::result<xstate::run_outcome>& ran) {
    return ran.has_value() && *ran == xstate::run_outcome::settled;
}

}  // namespace

int main() {
    // tag::example[]
    xstate::implementations dog_implementations;
    dog_implementations.actions.emplace("count", xstate::assign_action{.assignment = count});
    const boost::json::value dog_config = boost::json::parse(R"({
        "id": "dog",
        "context": {"fetched": 0},
        "on": {"fetch": {"actions": "count"}}
    })");
    const xstate::result<xstate::machine> dog =
        xstate::create_machine(dog_config, dog_implementations);
    if (!dog.has_value()) {
        return 1;
    }

    const xstate::send_to_action throw_ball{
        .target = "dog",
        .event = fetch,
        .id = std::nullopt,
        .delay = std::nullopt,
    };
    xstate::implementations owner_implementations;
    owner_implementations.actions.emplace("throw", throw_ball);
    owner_implementations.actors.emplace("dog", xstate::machine_actor{*dog});
    const boost::json::value owner_config = boost::json::parse(R"({
        "id": "owner",
        "invoke": {"id": "dog", "src": "dog"},
        "on": {"play": {"actions": ["throw", "throw"]}}
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
    const std::optional<xstate::actor_ref> dog_actor = system.child_of(*owner_actor, "dog");
    if (!dog_actor.has_value()) {
        return 1;
    }
    const xstate::result<void> subscribed =
        system.subscribe(*dog_actor, [](const xstate::snapshot& snapshot) {
            std::cout << "dog: " << boost::json::serialize(snapshot.context) << '\n';
        });
    if (!subscribed.has_value()) {
        return 1;
    }
    if (!settled(system.send(*owner_actor, {.type = "play", .payload = {}}))) {
        return 1;
    }
    // end::example[]
    return 0;
}
