// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 What the tests of xstate's actor layer share: machines made from their
 configs, the actions that send, raise and emit events, and the actors of a
 system started and found.

 Tip: a check a helper cannot go on without ends the program
 (require.hpp); a helper that a loop of a case calls takes the loop's item
 as a note, which a failed check prints after its own message.
*/
#ifndef WEBCPP_TEST_XSTATE_ACTOR_HELPERS_HPP
#define WEBCPP_TEST_XSTATE_ACTOR_HELPERS_HPP

#include <webcpp/xstate/actors.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "require.hpp"
#include "vocabulary.hpp"

namespace webcpp::test::xstate_actors {

namespace xstate = webcpp::xstate;
namespace xactor = webcpp::xactor;
namespace vocabulary = webcpp::test::xstate_vocabulary;

/** The JSON value `text` holds. */
inline boost::json::value parsed(std::string_view text) {
    boost::system::error_code failed;
    boost::json::value value = boost::json::parse(text, failed);
    require(noted(BOOST_TEST(!failed), "not JSON: ", text));
    return value;
}

/** A machine from its config, with the vocabulary's assigns and guards and `registry`'s additions.
 */
inline xstate::machine machine_of(std::string_view config,
                                  xstate::implementations registry = vocabulary::of_case({})) {
    xstate::result<xstate::machine> made =
        xstate::create_machine(parsed(config), std::move(registry));
    require(noted(BOOST_TEST(made.has_value()), "refused: ", config));
    return std::move(*made);
}

/** An event of `type` without a payload. */
inline xstate::event named(std::string_view type) {
    return {.type = std::string(type), .payload = {}};
}

/** The custom actions an actor system hands over, by type, in order. */
struct action_record {
    std::vector<std::string> types;
};

/** Records into `record` the custom actions `system` hands over; a failed check names `note`. */
template <class... Note>
void record_actions(xstate::actor_system& system, action_record& record, const Note&... note) {
    const xstate::result<void> acting =
        system.on_action([&record](xstate::actor_ref, const xstate::action& done) {
            record.types.push_back(done.type);
        });
    require(noted(BOOST_TEST(acting.has_value()), note...));
}

inline constexpr xactor::budgets plenty{.fuel = 10'000};

/** The vocabulary's implementations with `actors` added, each a machine actor. */
inline xstate::implementations with_actors(
    std::vector<std::pair<std::string, xstate::machine>> actors,
    xstate::implementations registry = vocabulary::of_case({})) {
    for (auto& [src, logic] : actors) {
        registry.actors.emplace(src, xstate::machine_actor{std::move(logic)});
    }
    return registry;
}

/** An event maker that always makes `type` with `payload`. */
inline xstate::event_maker making(std::string type, boost::json::object payload = {}) {
    return [type = std::move(type), payload = std::move(payload)](
               const xstate::action_args&) -> xstate::result<xstate::event> {
        return xstate::event{.type = type, .payload = payload};
    };
}

/** A sendTo of `type` to `target`, at once. */
inline xstate::send_to_action sending(std::string target, std::string type) {
    return xstate::send_to_action{
        .target = std::move(target),
        .event = making(std::move(type)),
        .id = std::nullopt,
        .delay = std::nullopt,
    };
}

/** A sendParent of `type` with `payload`, at once. */
inline xstate::send_parent_action sending_up(std::string type, boost::json::object payload = {}) {
    return xstate::send_parent_action{
        .event = making(std::move(type), std::move(payload)),
        .id = std::nullopt,
        .delay = std::nullopt,
    };
}

/** A started actor of `system` that runs `logic`; a failed check names `note`. */
template <class... Note>
xstate::actor_ref started(xstate::actor_system& system, const xstate::machine& logic,
                          const xstate::actor_options& options = xstate::actor_options(),
                          const Note&... note) {
    const xstate::result<xstate::actor_ref> actor = system.create_actor(logic, options);
    require(noted(BOOST_TEST(actor.has_value()), note...));
    require(noted(BOOST_TEST(system.start(*actor).has_value()), note...));
    return *actor;
}

/** The child `id` of `parent`; a failed check names `note` after the child. */
template <class... Note>
xstate::actor_ref child(const xstate::actor_system& system, xstate::actor_ref parent,
                        std::string_view id, const Note&... note) {
    const std::optional<xstate::actor_ref> found = system.child_of(parent, id);
    require(noted(noted(BOOST_TEST(found.has_value()), "no child ", id), note...));
    return *found;
}

/** The vocabulary's implementations with `fetchUser`, a host actor. */
inline xstate::implementations with_host() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actors.emplace("fetchUser", xstate::host_actor{});
    return registry;
}

/** An emit of `type`. */
inline xstate::emit_action emitting(std::string type) {
    return xstate::emit_action{.event = making(std::move(type))};
}

/** Registers `one` and `two`, raises of A and B, each a microstep of its own. */
inline void add_raises(xstate::implementations& registry) {
    for (const auto& [name, type] : {std::pair{"one", "A"}, std::pair{"two", "B"}}) {
        registry.actions.emplace(name, xstate::raise_action{
                                           .event = making(type),
                                           .id = std::nullopt,
                                           .delay = std::nullopt,
                                       });
    }
}

/** Resumes until every actor has settled; false when a call fails or it never settles. */
inline bool settles(xstate::actor_system& system, xstate::result<xstate::run_outcome> outcome) {
    for (int resumes = 0; resumes < 100'000; ++resumes) {
        if (!outcome.has_value()) {
            return false;
        }
        if (*outcome == xstate::run_outcome::settled) {
            return true;
        }
        outcome = system.resume();
    }
    return false;
}

}  // namespace webcpp::test::xstate_actors

#endif  // WEBCPP_TEST_XSTATE_ACTOR_HELPERS_HPP
