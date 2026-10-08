// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Tests what the host of xstate's actors sees and answers
 (doc: #xstate-on-xactor): the host actors that stand for XState's promises,
 their requests resolved and rejected, and the callbacks that hand over
 custom actions, logs, snapshots and emitted events, which cannot call the
 system back.

 Tip: XState's own actor tests are ported as cases against the oracle
 (actors_cases_test.cpp); this file and the other actors_*_test.cpp cover
 what the oracle cannot see.
*/

#include <webcpp/xstate/actors.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "actor_helpers.hpp"
#include "vocabulary.hpp"

namespace xstate = webcpp::xstate;
namespace xactor = webcpp::xactor;
namespace vocabulary = webcpp::test::xstate_vocabulary;

using webcpp::test::xstate_actors::action_record;
using webcpp::test::xstate_actors::add_raises;
using webcpp::test::xstate_actors::child;
using webcpp::test::xstate_actors::emitting;
using webcpp::test::xstate_actors::machine_of;
using webcpp::test::xstate_actors::named;
using webcpp::test::xstate_actors::parsed;
using webcpp::test::xstate_actors::plenty;
using webcpp::test::xstate_actors::record_actions;
using webcpp::test::xstate_actors::sending;
using webcpp::test::xstate_actors::started;
using webcpp::test::xstate_actors::with_actors;
using webcpp::test::xstate_actors::with_host;

namespace {

void custom_actions_and_logs_reach_their_callbacks_in_order() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("note", xstate::log_action{.value = std::nullopt, .label = "here"});
    const xstate::machine noisy =
        machine_of(R"({"context": {}, "entry": ["first", "note", "second"]})", std::move(registry));
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
    const xstate::actor_ref actor = system.create_actor(noisy).value();
    if (!BOOST_TEST(system.start(actor).has_value())) {
        return;
    }
    const std::vector<std::string> custom{"first", "second"};
    BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), custom.begin(), custom.end());
    if (!BOOST_TEST_EQ(labels.size(), 1U)) {
        return;
    }
    BOOST_TEST_EQ(labels.front(), "here");
}

constexpr std::string_view fetching = R"({
    "context": {},
    "initial": "loading",
    "states": {
        "loading": {
            "invoke": {
                "src": "fetchUser",
                "id": "fetch",
                "input": {"userId": 42},
                "onDone": {
                    "target": "loaded",
                    "actions": [
                        {"type": "set_from_event", "params": {"key": "user", "from": "output"}}
                    ]
                },
                "onError": {
                    "target": "failed",
                    "actions": [
                        {"type": "set_from_event", "params": {"key": "reason", "from": "error"}}
                    ]
                }
            },
            "on": {"CANCEL": "cancelled"}
        },
        "loaded": {},
        "failed": {},
        "cancelled": {}
    }
})";

void invoking_a_host_actor_asks_the_host_with_its_input() {
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, machine_of(fetching, with_host()));
    const std::vector<xstate::host_request> requests = system.host_requests();
    if (!BOOST_TEST_EQ(requests.size(), 1U)) {
        return;
    }
    BOOST_TEST(requests.front().actor == child(system, root, "fetch"));
    BOOST_TEST(requests.front().parent == root);
    BOOST_TEST_EQ(requests.front().id, "fetch");
    BOOST_TEST_EQ(requests.front().src, "fetchUser");
    BOOST_TEST(requests.front().input == std::optional(parsed(R"({"userId": 42})")));
}

void resolving_a_request_takes_on_done_with_the_output() {
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, machine_of(fetching, with_host()));
    const xstate::host_request request = system.host_requests().front();
    if (!BOOST_TEST(system.resolve(request, parsed(R"({"name": "ana"})")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("loaded")"));
    BOOST_TEST_EQ(system.snapshot_of(root)->context, parsed(R"({"user": {"name": "ana"}})"));
    BOOST_TEST(system.host_requests().empty());
    BOOST_TEST(system.status_of(request.actor).value() == xactor::status::done);
}

void rejecting_a_request_takes_on_error_with_the_error() {
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, machine_of(fetching, with_host()));
    const xstate::host_request request = system.host_requests().front();
    if (!BOOST_TEST(system.reject(request, parsed(R"({"message": "boom"})")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("failed")"));
    BOOST_TEST_EQ(system.snapshot_of(root)->context, parsed(R"({"reason": {"message": "boom"}})"));
    BOOST_TEST(system.status_of(request.actor).value() == xactor::status::error);
}

void an_unhandled_rejection_fails_the_parent_with_the_error() {
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(
        system, machine_of(R"({"invoke": {"src": "fetchUser", "id": "fetch"}})", with_host()));
    if (!BOOST_TEST(
            system.reject(system.host_requests().front(), parsed(R"("offline")")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(root).value() == xactor::status::error);
    BOOST_TEST_EQ(system.snapshot_of(root)->error_value.value(), parsed(R"("offline")"));
}

// A request whose invoking state was left is dropped, and an answer that comes
// after it is refused and changes nothing.
void a_request_whose_actor_was_stopped_is_dropped_and_a_late_answer_changes_nothing() {
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, machine_of(fetching, with_host()));
    const xstate::host_request request = system.host_requests().front();
    if (!BOOST_TEST(system.send(root, named("CANCEL")).has_value())) {
        return;
    }
    BOOST_TEST(system.host_requests().empty());
    const xstate::result<xstate::run_outcome> late =
        system.resolve(request, parsed(R"({"name": "ana"})"));
    if (!BOOST_TEST(!late.has_value())) {
        return;
    }
    BOOST_TEST_EQ(late.error(), xactor::make_error_code(xactor::errc::invalid_argument));
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("cancelled")"));
    BOOST_TEST(system.status_of(request.actor).value() == xactor::status::stopped);
}

// As in XState, a failed actor's children keep running: its host child's
// request stays pending.
void a_failed_parent_leaves_its_host_child_pending() {
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, machine_of(R"({
        "context": {},
        "invoke": {"src": "fetchUser", "id": "fetch"},
        "on": {"BREAK": {"actions": ["fail"]}}
    })",
                                                              with_host()));
    if (!BOOST_TEST(system.send(root, named("BREAK")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(root).value() == xactor::status::error);
    BOOST_TEST_EQ(system.host_requests().size(), 1U);
}

// XState's observers get next for every processed event while the actor is
// active or done, a transition taken or not, and none for an error.
void a_subscriber_sees_each_settled_snapshot_once() {
    const xstate::machine toggle = machine_of(R"({
        "context": {},
        "initial": "off",
        "states": {
            "off": {"on": {"TOGGLE": "on"}},
            "on": {
                "on": {
                    "TOGGLE": "off",
                    "END": "end",
                    "BREAK": {"actions": ["fail"]}
                }
            },
            "end": {"type": "final"}
        }
    })");
    xstate::actor_system system(plenty);
    const xstate::actor_ref actor = system.create_actor(toggle).value();
    std::vector<std::string> seen;
    const xstate::result<void> subscribed =
        system.subscribe(actor, [&seen](const xstate::snapshot& settled) {
            seen.push_back(boost::json::serialize(settled.value));
        });
    if (!BOOST_TEST(subscribed.has_value())) {
        return;
    }
    if (!BOOST_TEST(system.start(actor).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(actor, named("NOTHING")).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(actor, named("TOGGLE")).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(actor, named("END")).has_value())) {
        return;
    }
    const std::vector<std::string> expected{R"("off")", R"("off")", R"("on")", R"("end")"};
    BOOST_TEST_ALL_EQ(seen.begin(), seen.end(), expected.begin(), expected.end());

    const xstate::actor_ref other = system.create_actor(toggle).value();
    std::size_t notified = 0;
    const xstate::result<void> counting =
        system.subscribe(other, [&notified](const xstate::snapshot&) { ++notified; });
    if (!BOOST_TEST(counting.has_value())) {
        return;
    }
    if (!BOOST_TEST(system.start(other).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(other, named("TOGGLE")).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(other, named("BREAK")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(notified, 2U);
}

// An emitted event of the wildcard type reaches the listeners once, and its
// actor stays active: XState emits it too, but hands it twice to a listener
// on '*', looked up both as the event's type and as the wildcard
// (doc: #differences-emit).
void an_emitted_event_of_the_wildcard_type_reaches_the_listeners_once() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("emit_star", emitting("*"));
    const xstate::machine noisy = machine_of(R"({
        "on": {"GO": {"actions": ["emit_star"]}}
    })",
                                             std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref actor = system.create_actor(noisy).value();
    std::vector<std::string> heard;
    const xstate::result<void> listening = system.on_emitted(
        actor, [&heard](const xstate::event& emitted) { heard.push_back(emitted.type); });
    if (!BOOST_TEST(listening.has_value())) {
        return;
    }
    if (!BOOST_TEST(system.start(actor).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(actor, named("GO")).has_value())) {
        return;
    }
    const std::vector<std::string> expected{"*"};
    BOOST_TEST_ALL_EQ(heard.begin(), heard.end(), expected.begin(), expected.end());
    BOOST_TEST(system.status_of(actor).value() == xactor::status::active);
}

void emitted_events_reach_their_listeners_in_order() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("emit_a", emitting("A"));
    registry.actions.emplace("emit_b", emitting("B"));
    registry.actions.emplace("emit_c", emitting("C"));
    const xstate::machine noisy = machine_of(R"({
        "entry": ["emit_a"],
        "on": {
            "GO": {"actions": ["emit_b", "emit_c"]}
        }
    })",
                                             std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref actor = system.create_actor(noisy).value();
    std::vector<std::string> heard;
    const xstate::result<void> listening = system.on_emitted(
        actor, [&heard](const xstate::event& emitted) { heard.push_back(emitted.type); });
    if (!BOOST_TEST(listening.has_value())) {
        return;
    }
    if (!BOOST_TEST(system.start(actor).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(actor, named("GO")).has_value())) {
        return;
    }
    const std::vector<std::string> expected{"A", "B", "C"};
    BOOST_TEST_ALL_EQ(heard.begin(), heard.end(), expected.begin(), expected.end());
}

// XState runs a running actor's custom actions as they are resolved and its
// emits after the macrostep, so every custom action comes first.
void a_macrostep_s_custom_actions_come_before_its_emits() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("emit_e", emitting("E"));
    const xstate::machine mixed =
        machine_of(R"({"entry": ["first", "emit_e", "second"]})", std::move(registry));
    xstate::actor_system system(plenty);
    std::vector<std::string> order;
    const xstate::result<void> acting = system.on_action(
        [&order](xstate::actor_ref, const xstate::action& done) { order.push_back(done.type); });
    if (!BOOST_TEST(acting.has_value())) {
        return;
    }
    const xstate::actor_ref actor = system.create_actor(mixed).value();
    const xstate::result<void> listening = system.on_emitted(
        actor,
        [&order](const xstate::event& emitted) { order.push_back("emitted " + emitted.type); });
    if (!BOOST_TEST(listening.has_value())) {
        return;
    }
    if (!BOOST_TEST(system.start(actor).has_value())) {
        return;
    }
    const std::vector<std::string> expected{"first", "second", "emitted E"};
    BOOST_TEST_ALL_EQ(order.begin(), order.end(), expected.begin(), expected.end());
}

// XState resolves a promise without a value as a done event without output.
void a_host_answer_without_an_output_sends_a_done_event_without_one() {
    xstate::actor_system system(plenty);
    std::vector<xstate::event> heard;
    const xstate::result<void> inspecting = system.inspect(xstate::inspector{
        .started = nullptr,
        .settled = [&heard](xstate::actor_ref, const xstate::machine&, const xstate::event& cause,
                            const xstate::snapshot&) { heard.push_back(cause); },
        .emitted = nullptr,
    });
    if (!BOOST_TEST(inspecting.has_value())) {
        return;
    }
    started(system, machine_of(fetching, with_host()));
    if (!BOOST_TEST(system.resolve(system.host_requests().front(), std::nullopt).has_value())) {
        return;
    }
    if (!BOOST_TEST_EQ(heard.size(), 2U)) {
        return;
    }
    BOOST_TEST_EQ(heard.back().type, "xstate.done.actor.fetch");
    BOOST_TEST(!heard.back().payload.contains("output"));
}

// The host's inputs wait while an actor is parked, so a parked macrostep
// settles before them, as XState runs each input to its end: a rejection of
// a request the parked macrostep stops is then ignored.
void a_host_answer_waits_for_a_parked_parent_and_is_dropped_if_it_stopped_the_child() {
    xstate::implementations registry = with_host();
    add_raises(registry);
    const xstate::machine parent = machine_of(R"({
        "context": {},
        "initial": "loading",
        "states": {
            "loading": {
                "invoke": {"src": "fetchUser", "id": "fetch"},
                "on": {
                    "CANCEL": {"target": "cancelled", "actions": ["one", "two"]}
                }
            },
            "cancelled": {}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(xactor::budgets{.fuel = 2});
    const xstate::actor_ref root = started(system, parent);
    const xstate::host_request request = system.host_requests().front();
    // CANCEL's macrostep, three microsteps with the two raised events, pays
    // two and parks before its effects, the stop of fetch among them.
    BOOST_TEST(system.send(root, named("CANCEL")).value() == xstate::run_outcome::out_of_fuel);
    BOOST_TEST(system.reject(request, parsed(R"("offline")")).value() ==
               xstate::run_outcome::out_of_fuel);
    BOOST_TEST(system.resume().value() == xstate::run_outcome::settled);
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("cancelled")"));
    BOOST_TEST(system.status_of(root).value() == xactor::status::active);
}

// A log whose value computes JavaScript's undefined reaches the host with
// no value, as XState's logger receives undefined, and one that computes
// null with null.
void a_log_whose_value_is_undefined_reaches_the_host_without_one() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace(
        "say_nothing",
        xstate::log_action{
            .value = [](const xstate::action_args&)
                -> xstate::result<std::optional<boost::json::value>> { return std::nullopt; },
            .label = "nothing",
        });
    registry.actions.emplace("say_null",
                             xstate::log_action{
                                 .value = [](const xstate::action_args&)
                                     -> xstate::result<std::optional<boost::json::value>> {
                                     return boost::json::value(nullptr);
                                 },
                                 .label = "null",
                             });
    const xstate::machine talking = machine_of(R"({
        "on": {"GO": {"actions": ["say_nothing", "say_null"]}}
    })",
                                               std::move(registry));
    xstate::actor_system system(plenty);
    std::vector<std::string> heard;
    const xstate::result<void> logging = system.on_log(
        [&heard](xstate::actor_ref, const boost::json::value* value, std::string_view label) {
            heard.push_back(std::string(label) + ": " +
                            (value == nullptr ? "none" : boost::json::serialize(*value)));
        });
    if (!BOOST_TEST(logging.has_value())) {
        return;
    }
    const xstate::actor_ref actor = started(system, talking);
    if (!BOOST_TEST(system.send(actor, named("GO")).has_value())) {
        return;
    }
    const std::vector<std::string> expected{"nothing: none", "null: null"};
    BOOST_TEST_ALL_EQ(heard.begin(), heard.end(), expected.begin(), expected.end());
}

// A callback runs inside an actor's turn, so a call that changes the system
// made from it is refused with invalid_argument and changes nothing, where
// XState lets a subscriber send and handles the event after the update in
// progress; what a callback reads is free.
void a_callback_cannot_call_the_system_back() {
    const xstate::machine toggle = machine_of(R"({
        "initial": "off",
        "states": {
            "off": {"on": {"TOGGLE": "on"}},
            "on": {"on": {"TOGGLE": "off"}}
        }
    })");
    xstate::actor_system system(plenty);
    const xstate::actor_ref actor = system.create_actor(toggle).value();
    std::vector<std::string> refused;
    std::vector<std::string> seen;
    const auto note = [&refused](std::string_view call, bool failed) {
        if (failed) {
            refused.emplace_back(call);
        }
    };
    const auto calls_back = [&](const xstate::snapshot& settled) {
        seen.push_back(boost::json::serialize(settled.value));
        note("start", !system.start(actor).has_value());
        note("send", !system.send(actor, named("TOGGLE")).has_value());
        note("clock_tick", !system.clock_tick(0).has_value());
        note("resume", !system.resume().has_value());
        note("run_until_idle", !system.run_until_idle().has_value());
        note("stop", !system.stop(actor).has_value());
        note("create_actor", !system.create_actor(toggle).has_value());
        note("subscribe", !system.subscribe(actor, {}).has_value());
        note("on_emitted", !system.on_emitted(actor, {}).has_value());
        note("on_action", !system.on_action({}).has_value());
        note("on_log", !system.on_log({}).has_value());
        note("inspect", !system.inspect({}).has_value());
        note("snapshot_of", system.snapshot_of(actor) == nullptr);
        note("status_of", !system.status_of(actor).has_value());
    };
    if (!BOOST_TEST(system.subscribe(actor, calls_back).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.start(actor).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(actor, named("TOGGLE")).has_value())) {
        return;
    }
    const std::vector<std::string> values{R"("off")", R"("on")"};
    BOOST_TEST_ALL_EQ(seen.begin(), seen.end(), values.begin(), values.end());
    const std::vector<std::string> each{
        "start",        "send",      "clock_tick", "resume",    "run_until_idle", "stop",
        "create_actor", "subscribe", "on_emitted", "on_action", "on_log",         "inspect",
    };
    std::vector<std::string> expected = each;
    expected.insert(expected.end(), each.begin(), each.end());
    BOOST_TEST_ALL_EQ(refused.begin(), refused.end(), expected.begin(), expected.end());
    BOOST_TEST(system.status_of(actor).value() == xactor::status::active);
    BOOST_TEST_EQ(system.snapshot_of(actor)->value, parsed(R"("on")"));
}

// The same holds for the answers to a host request, which deliver as a send
// does: a listener's answer is refused and the request stays pending.
void a_callback_cannot_answer_a_host_request() {
    const xstate::machine waiting = machine_of(R"({
        "initial": "loading",
        "states": {
            "loading": {
                "invoke": {"src": "fetchUser", "id": "fetch"},
                "on": {"PING": {}}
            }
        }
    })",
                                               with_host());
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, waiting);
    if (!BOOST_TEST_EQ(system.host_requests().size(), 1U)) {
        return;
    }
    const xstate::host_request request = system.host_requests().front();
    std::vector<std::string> refused;
    const auto calls_back = [&](const xstate::snapshot&) {
        if (!system.resolve(request, parsed(R"({"name": "ana"})")).has_value()) {
            refused.emplace_back("resolve");
        }
        if (!system.reject(request, parsed(R"("offline")")).has_value()) {
            refused.emplace_back("reject");
        }
    };
    if (!BOOST_TEST(system.subscribe(root, calls_back).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(root, named("PING")).has_value())) {
        return;
    }
    const std::vector<std::string> expected{"resolve", "reject"};
    BOOST_TEST_ALL_EQ(refused.begin(), refused.end(), expected.begin(), expected.end());
    BOOST_TEST_EQ(system.host_requests().size(), 1U);
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("loading")"));
}

// A callback's clock_tick is refused before it moves the time, so a timer
// due by then stays armed; applied from inside the run, the tick would fire
// it there, though the call reported a refusal.
void a_callback_cannot_move_the_time() {
    const xstate::machine waiting = machine_of(R"({
        "initial": "waiting",
        "states": {
            "waiting": {"after": {"1000": "late"}, "on": {"PING": {}}},
            "late": {}
        }
    })");
    xstate::actor_system system(plenty);
    const xstate::actor_ref actor = started(system, waiting);
    std::size_t refused = 0;
    const auto calls_back = [&](const xstate::snapshot&) {
        if (!system.clock_tick(5000).has_value()) {
            ++refused;
        }
    };
    if (!BOOST_TEST(system.subscribe(actor, calls_back).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(actor, named("PING")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(refused, 1U);
    BOOST_TEST_EQ(system.snapshot_of(actor)->value, parsed(R"("waiting")"));
    if (!BOOST_TEST(system.clock_tick(1000).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(actor)->value, parsed(R"("late")"));
}

// A callback's resume() is refused before it hands a parked actor more
// fuel: the sibling still parked when a child settles gets only the
// executions the host gives it.
void a_callback_cannot_resume_a_parked_actor() {
    // X runs twenty microsteps in `slow`, more than two executions pay for,
    // and two in `quick`.
    const xstate::machine slow = machine_of(R"({
        "context": {"n": 0},
        "initial": "idle",
        "states": {
            "idle": {"on": {"X": "counting"}},
            "counting": {
                "always": {
                    "guard": {
                        "type": "compare",
                        "params": {"left": {"$context": "n"}, "op": "<", "right": 20}
                    },
                    "actions": [{"type": "increment", "params": {"key": "n"}}]
                }
            }
        }
    })",
                                            vocabulary::of_case({}));
    const xstate::machine quick = machine_of(R"({
        "initial": "idle",
        "states": {
            "idle": {"on": {"X": "q1"}},
            "q1": {"always": "q2"},
            "q2": {}
        }
    })");
    xstate::implementations registry = with_actors({{"slow", slow}, {"quick", quick}});
    registry.actions.emplace("x_to_slow", sending("slow", "X"));
    registry.actions.emplace("x_to_quick", sending("quick", "X"));
    const xstate::machine parent = machine_of(R"({
        "invoke": [
            {"src": "slow", "id": "slow"},
            {"src": "quick", "id": "quick"}
        ],
        "on": {"GO": {"actions": ["x_to_slow", "x_to_quick"]}}
    })",
                                              std::move(registry));
    // The start costs exactly 9: the root's initial microstep and, for each
    // child, its construction, its initial microstep, its answer and its start.
    xstate::actor_system system(xactor::budgets{.fuel = 9});
    const xstate::actor_ref root = started(system, parent);
    const xstate::actor_ref counter = child(system, root, "slow");
    std::size_t refused = 0;
    const auto calls_back = [&](const xstate::snapshot&) {
        if (!system.resume().has_value()) {
            ++refused;
        }
    };
    if (!BOOST_TEST(system.subscribe(child(system, root, "quick"), calls_back).has_value())) {
        return;
    }
    BOOST_TEST(system.send(root, named("GO")).value() == xstate::run_outcome::out_of_fuel);
    // The host's first resume pays `quick` to the end and `counter` for nine
    // more microsteps: `quick`'s listener cannot buy it more.
    BOOST_TEST(system.resume().value() == xstate::run_outcome::out_of_fuel);
    BOOST_TEST_EQ(refused, 1U);
    BOOST_TEST(system.resume().value() == xstate::run_outcome::settled);
    BOOST_TEST_EQ(system.snapshot_of(counter)->context, parsed(R"({"n": 20})"));
}

// An empty listener is no listener, as an empty on_action callback is: the
// system never calls it, where calling it would abort without exceptions.
// A listener of an address that names no actor is refused, as a start or a
// send to it is, rather than kept to hear an actor created later there.
void a_listener_of_no_actor_is_refused() {
    xstate::actor_system system(plenty);
    const xstate::actor_ref nobody{.value = 999};
    const xstate::result<void> subscribed =
        system.subscribe(nobody, [](const xstate::snapshot&) {});
    const xstate::result<void> listening = system.on_emitted(nobody, [](const xstate::event&) {});
    if (!BOOST_TEST(!subscribed.has_value())) {
        return;
    }
    BOOST_TEST_EQ(subscribed.error(), xactor::make_error_code(xactor::errc::invalid_argument));
    if (!BOOST_TEST(!listening.has_value())) {
        return;
    }
    BOOST_TEST_EQ(listening.error(), xactor::make_error_code(xactor::errc::invalid_argument));
}

void an_empty_listener_is_never_called() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("shout", emitting("SHOUT"));
    const xstate::machine shouting = machine_of(R"({
        "context": {},
        "on": {"GO": {"actions": ["shout"]}}
    })",
                                                std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref actor = system.create_actor(shouting).value();
    if (!BOOST_TEST(system.subscribe(actor, {}).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.on_emitted(actor, nullptr).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.start(actor).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(actor, named("GO")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(actor).value() == xactor::status::active);
}

}  // namespace

int main() {
    custom_actions_and_logs_reach_their_callbacks_in_order();
    invoking_a_host_actor_asks_the_host_with_its_input();
    resolving_a_request_takes_on_done_with_the_output();
    rejecting_a_request_takes_on_error_with_the_error();
    an_unhandled_rejection_fails_the_parent_with_the_error();
    a_request_whose_actor_was_stopped_is_dropped_and_a_late_answer_changes_nothing();
    a_failed_parent_leaves_its_host_child_pending();
    a_subscriber_sees_each_settled_snapshot_once();
    an_emitted_event_of_the_wildcard_type_reaches_the_listeners_once();
    emitted_events_reach_their_listeners_in_order();
    a_macrostep_s_custom_actions_come_before_its_emits();
    a_host_answer_without_an_output_sends_a_done_event_without_one();
    a_host_answer_waits_for_a_parked_parent_and_is_dropped_if_it_stopped_the_child();
    a_log_whose_value_is_undefined_reaches_the_host_without_one();
    a_callback_cannot_call_the_system_back();
    a_callback_cannot_answer_a_host_request();
    a_callback_cannot_move_the_time();
    a_callback_cannot_resume_a_parked_actor();
    a_listener_of_no_actor_is_refused();
    an_empty_listener_is_never_called();
    return boost::report_errors();
}
