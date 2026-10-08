// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Tests the lifecycle of xstate's actors (doc: #xstate-on-xactor): a machine
 run as an actor on xactor, created, started, sent events, stopped, done or
 failed, what a failing macrostep hands over, and the ids the system names
 its actors with.

 Tip: XState's own actor tests are ported as cases against the oracle
 (actors_cases_test.cpp); this file and the other actors_*_test.cpp cover
 what the oracle cannot see.
*/

#include <webcpp/xstate/actors.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "actor_helpers.hpp"
#include "require.hpp"
#include "vocabulary.hpp"

namespace xstate = webcpp::xstate;
namespace xactor = webcpp::xactor;
namespace vocabulary = webcpp::test::xstate_vocabulary;

using webcpp::test::noted;
using webcpp::test::xstate_actors::action_record;
using webcpp::test::xstate_actors::child;
using webcpp::test::xstate_actors::machine_of;
using webcpp::test::xstate_actors::named;
using webcpp::test::xstate_actors::parsed;
using webcpp::test::xstate_actors::plenty;
using webcpp::test::xstate_actors::record_actions;
using webcpp::test::xstate_actors::sending_up;
using webcpp::test::xstate_actors::started;
using webcpp::test::xstate_actors::with_actors;
using webcpp::test::xstate_actors::with_host;

namespace {

void an_actor_starts_takes_events_and_shows_its_snapshot() {
    const xstate::machine toggle = machine_of(R"({
        "id": "toggle",
        "initial": "off",
        "states": {
            "off": {"on": {"TOGGLE": "on"}},
            "on": {"on": {"TOGGLE": "off"}}
        }
    })");
    xstate::actor_system system(plenty);
    const xstate::result<xstate::actor_ref> actor = system.create_actor(toggle);
    if (!BOOST_TEST(actor.has_value())) {
        return;
    }
    BOOST_TEST(system.snapshot_of(*actor) == nullptr);
    BOOST_TEST(system.start(*actor).value() == xstate::run_outcome::settled);
    if (!BOOST_TEST(system.snapshot_of(*actor) != nullptr)) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(*actor)->value, parsed(R"("off")"));
    BOOST_TEST(system.send(*actor, named("TOGGLE")).value() == xstate::run_outcome::settled);
    BOOST_TEST_EQ(system.snapshot_of(*actor)->value, parsed(R"("on")"));
    BOOST_TEST(system.status_of(*actor).value() == xactor::status::active);
}

void events_sent_before_the_start_run_after_the_initial_macrostep() {
    const xstate::machine counter = machine_of(R"({
        "context": {"count": 0},
        "on": {
            "ADD": {
                "actions": [
                    {"type": "increment", "params": {"key": "count"}}
                ]
            }
        }
    })");
    xstate::actor_system system(plenty);
    const xstate::actor_ref actor = system.create_actor(counter).value();
    if (!BOOST_TEST(system.send(actor, named("ADD")).has_value())) {
        return;
    }
    BOOST_TEST(system.snapshot_of(actor) == nullptr);
    if (!BOOST_TEST(system.start(actor).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(actor)->context, parsed(R"({"count": 1})"));
}

void a_machine_that_reaches_a_final_state_is_done_with_its_output() {
    const xstate::machine finite = machine_of(R"({
        "initial": "a",
        "output": {"ok": true},
        "states": {
            "a": {"on": {"END": "end"}},
            "end": {"type": "final"}
        }
    })");
    xstate::actor_system system(plenty);
    const xstate::actor_ref actor = system.create_actor(finite).value();
    if (!BOOST_TEST(system.start(actor).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(actor, named("END")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(actor).value() == xactor::status::done);
    BOOST_TEST(system.snapshot_of(actor)->status == xstate::status::done);
    BOOST_TEST_EQ(system.snapshot_of(actor)->output.value(), parsed(R"({"ok": true})"));
}

void an_implementation_that_fails_makes_the_actor_fail() {
    const xstate::machine failing = machine_of(R"({
        "context": {},
        "on": {"BREAK": {"actions": ["fail"]}}
    })");
    xstate::actor_system system(plenty);
    const xstate::actor_ref actor = system.create_actor(failing).value();
    if (!BOOST_TEST(system.start(actor).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(actor, named("BREAK")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(actor).value() == xactor::status::error);
    BOOST_TEST(system.snapshot_of(actor)->status == xstate::status::error);
    BOOST_TEST_EQ(system.snapshot_of(actor)->error.message(), "implementation_failed");
}

void a_stopped_actor_receives_nothing_more() {
    const xstate::machine toggle = machine_of(R"({
        "initial": "off",
        "states": {
            "off": {"on": {"TOGGLE": "on"}},
            "on": {}
        }
    })");
    xstate::actor_system system(plenty);
    const xstate::actor_ref actor = system.create_actor(toggle).value();
    if (!BOOST_TEST(system.start(actor).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.stop(actor).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(actor).value() == xactor::status::stopped);
    if (!BOOST_TEST(system.send(actor, named("TOGGLE")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(actor)->value, parsed(R"("off")"));
}

void an_address_that_names_no_actor_is_refused() {
    xstate::actor_system system(plenty);
    BOOST_TEST(!system.start(xstate::actor_ref{.value = 7}).has_value());
    BOOST_TEST(!system.send(xstate::actor_ref{.value = 7}, named("X")).has_value());
    BOOST_TEST(system.snapshot_of(xstate::actor_ref{.value = 7}) == nullptr);
}

// A resolution XState throws for ends the macrostep there: no custom action
// after it is handed over, and the actor fails with the snapshot the
// macrostep began from (doc: #xstate-invariant-a9).
void no_action_after_a_resolution_that_throws_is_handed_over() {
    xstate::implementations registry = with_host();
    registry.actions.emplace("up", sending_up("X"));
    registry.actions.emplace("claim", xstate::spawn_child_action{
                                          .src = "fetchUser",
                                          .id = "second",
                                          .system_id = "svc",
                                          .input = std::nullopt,
                                      });
    const xstate::machine throwing = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {
                "invoke": {"src": "fetchUser", "id": "first", "systemId": "svc"},
                "on": {
                    "UP": {"target": "b", "actions": ["before", "up", "after"]},
                    "CLAIM": {"actions": ["before", "claim", "after"]}
                }
            },
            "b": {"entry": ["in_b"]}
        }
    })",
                                                std::move(registry));
    for (const auto& [type, error] : std::vector<std::pair<std::string_view, std::string_view>>{
             {"UP", "unknown_target"},
             {"CLAIM", "system_id_taken"},
         }) {
        xstate::actor_system system(plenty);
        action_record record;
        record_actions(system, record, "event ", type);
        const xstate::actor_ref root =
            started(system, throwing, xstate::actor_options(), "event ", type);
        if (!noted(BOOST_TEST(system.send(root, named(type)).has_value()), "event ", type)) {
            return;
        }
        const std::vector<std::string> before{"before"};
        noted(BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), before.begin(),
                                before.end()),
              "event ", type);
        noted(BOOST_TEST(system.status_of(root).value() == xactor::status::error), "event ", type);
        noted(BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("a")")), "event ", type);
        noted(BOOST_TEST_EQ(system.snapshot_of(root)->error.message(), error), "event ", type);
    }
}

// XState runs an initial macrostep's custom actions on start(), so none
// when that macrostep fails; a later macrostep has run those of the
// microsteps before the failing one.
void an_initial_macrostep_that_fails_hands_over_no_custom_action() {
    const xstate::machine failing = machine_of(R"({
        "context": {},
        "initial": "a",
        "states": {
            "a": {"entry": ["first"], "always": "b"},
            "b": {"entry": ["fail"]}
        }
    })");
    xstate::actor_system system(plenty);
    action_record record;
    record_actions(system, record);
    const xstate::actor_ref actor = started(system, failing);
    BOOST_TEST(record.types.empty());
    BOOST_TEST(system.status_of(actor).value() == xactor::status::error);

    const xstate::machine later = machine_of(R"({
        "context": {},
        "initial": "idle",
        "states": {
            "idle": {"on": {"GO": "a"}},
            "a": {"entry": ["first"], "always": "b"},
            "b": {"entry": ["fail"]}
        }
    })");
    xstate::actor_system other(plenty);
    action_record heard;
    record_actions(other, heard);
    const xstate::actor_ref running = started(other, later);
    if (!BOOST_TEST(other.send(running, named("GO")).has_value())) {
        return;
    }
    const std::vector<std::string> first{"first"};
    BOOST_TEST_ALL_EQ(heard.types.begin(), heard.types.end(), first.begin(), first.end());
    BOOST_TEST(other.status_of(running).value() == xactor::status::error);
}

// A running actor's microstep that fails has handed over the custom actions
// and logs it resolved before the failure, as XState runs them at once, and
// nothing after it; an initial macrostep hands over none, as XState runs
// those on start().
void a_failing_microstep_hands_over_what_it_resolved_before_the_failure() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("note", xstate::log_action{.value = std::nullopt, .label = "here"});
    const xstate::machine failing = machine_of(R"({
        "context": {},
        "on": {"GO": {"actions": ["before", "note", "fail", "after"]}}
    })",
                                               std::move(registry));
    xstate::actor_system system(plenty);
    action_record record;
    record_actions(system, record);
    std::vector<std::string> labels;
    const xstate::result<void> logging =
        system.on_log([&labels](xstate::actor_ref, const boost::json::value*,
                                std::string_view label) { labels.emplace_back(label); });
    if (!BOOST_TEST(logging.has_value())) {
        return;
    }
    const xstate::actor_ref actor = started(system, failing);
    if (!BOOST_TEST(system.send(actor, named("GO")).has_value())) {
        return;
    }
    const std::vector<std::string> before{"before"};
    BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), before.begin(), before.end());
    const std::vector<std::string> logged{"here"};
    BOOST_TEST_ALL_EQ(labels.begin(), labels.end(), logged.begin(), logged.end());
    BOOST_TEST(system.status_of(actor).value() == xactor::status::error);
    BOOST_TEST_EQ(system.snapshot_of(actor)->error.message(), "implementation_failed");
}

// A root the host creates without an id is named by its session id, x:<n>,
// n the actors the system created before it, children and host actors
// included, as XState's counter numbers every actor; one with an id still
// takes a number. XState's own run: x:0, then x:3 after two children, then
// "mine" taking x:4, then x:5.
void a_root_without_an_id_is_named_by_its_session_id() {
    xstate::implementations registry = with_actors({{"child", machine_of(R"({})")}}, with_host());
    const xstate::machine parent = machine_of(R"({
        "invoke": [
            {"src": "child", "id": "a"},
            {"src": "fetchUser", "id": "b"}
        ]
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref first = started(system, parent);
    const xstate::actor_ref second = system.create_actor(machine_of(R"({})")).value();
    const xstate::actor_ref named =
        system.create_actor(machine_of(R"({})"), {.input = nullptr, .id = "mine", .system_id = {}})
            .value();
    const xstate::actor_ref third = system.create_actor(machine_of(R"({})")).value();
    BOOST_TEST_EQ(system.id_of(first), "x:0");
    BOOST_TEST_EQ(system.id_of(second), "x:3");
    BOOST_TEST_EQ(system.id_of(named), "mine");
    BOOST_TEST_EQ(system.id_of(third), "x:5");
}

// A spawn whose systemId claim is refused still takes a session id, as
// XState's constructor books one before the claim throws: XState's own run
// names the next root x:3, after the root, its invoke and the refused spawn.
void a_refused_claim_takes_a_session_id() {
    xstate::implementations registry = with_actors({{"child", machine_of(R"({})")}});
    registry.actions.emplace("spawn_b", xstate::spawn_child_action{
                                            .src = "child",
                                            .id = "b",
                                            .system_id = "x",
                                            .input = std::nullopt,
                                        });
    const xstate::machine parent = machine_of(R"({
        "invoke": {"src": "child", "id": "a", "systemId": "x"},
        "on": {"GO": {"actions": ["spawn_b"]}}
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref first = started(system, parent);
    if (!BOOST_TEST(system.send(first, named("GO")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(first).value() == xactor::status::error);
    const xstate::actor_ref second = system.create_actor(machine_of(R"({})")).value();
    BOOST_TEST_EQ(system.id_of(second), "x:3");
}

// The host stops only a root, as XState's actor.stop() refuses any other
// ("A non-root actor cannot be stopped directly"): a child is its parent's.
void the_host_stops_only_a_root() {
    const xstate::machine parent = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {"invoke": {"src": "worker", "id": "kid"}}
        }
    })",
                                              with_actors({{"worker", machine_of(R"({})")}}));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    const xstate::actor_ref kid = child(system, root, "kid");
    const xstate::result<std::vector<xstate::actor_ref>> refused = system.stop(kid);
    if (!BOOST_TEST(!refused.has_value())) {
        return;
    }
    BOOST_TEST_EQ(refused.error().message(), "invalid_argument");
    BOOST_TEST(system.status_of(kid).value() == xactor::status::active);
    BOOST_TEST(system.child_of(root, "kid") == std::optional(kid));
    if (!BOOST_TEST(system.stop(root).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(kid).value() == xactor::status::stopped);
}

}  // namespace

int main() {
    an_actor_starts_takes_events_and_shows_its_snapshot();
    events_sent_before_the_start_run_after_the_initial_macrostep();
    a_machine_that_reaches_a_final_state_is_done_with_its_output();
    an_implementation_that_fails_makes_the_actor_fail();
    a_stopped_actor_receives_nothing_more();
    an_address_that_names_no_actor_is_refused();
    no_action_after_a_resolution_that_throws_is_handed_over();
    an_initial_macrostep_that_fails_hands_over_no_custom_action();
    a_failing_microstep_hands_over_what_it_resolved_before_the_failure();
    a_root_without_an_id_is_named_by_its_session_id();
    a_refused_claim_takes_a_session_id();
    the_host_stops_only_a_root();
    return boost::report_errors();
}
