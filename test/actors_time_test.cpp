// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Tests the time and the fuel of xstate's actors (doc: #xstate-on-xactor):
 their timers, delayed events and cancels, the fuel each microstep costs, a
 macrostep that parks when the fuel runs out, and what waits for it.

 Tip: XState's own actor tests are ported as cases against the oracle
 (actors_cases_test.cpp); this file and the other actors_*_test.cpp cover
 what the oracle cannot see, fuel first.
*/

#include <webcpp/xstate/actors.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/json.hpp>

#include <cstdint>
#include <functional>
#include <map>
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
using webcpp::test::require;
using webcpp::test::xstate_actors::action_record;
using webcpp::test::xstate_actors::add_raises;
using webcpp::test::xstate_actors::child;
using webcpp::test::xstate_actors::emitting;
using webcpp::test::xstate_actors::machine_of;
using webcpp::test::xstate_actors::making;
using webcpp::test::xstate_actors::named;
using webcpp::test::xstate_actors::parsed;
using webcpp::test::xstate_actors::plenty;
using webcpp::test::xstate_actors::record_actions;
using webcpp::test::xstate_actors::sending;
using webcpp::test::xstate_actors::sending_up;
using webcpp::test::xstate_actors::settles;
using webcpp::test::xstate_actors::started;
using webcpp::test::xstate_actors::with_actors;
using webcpp::test::xstate_actors::with_host;

namespace {

// A macrostep the fuel cannot finish parks, an event that arrives meanwhile
// waits, and resume() finishes the macrostep before the event runs.
void a_parked_macrostep_settles_before_an_event_that_arrived_meanwhile() {
    // GO runs five microsteps: idle to a, then a to b, c, d and e eventlessly.
    const xstate::machine chain = machine_of(R"({
        "initial": "idle",
        "states": {
            "idle": {"on": {"GO": "a"}},
            "a": {"entry": ["enter_a"], "always": "b"},
            "b": {"entry": ["enter_b"], "always": "c"},
            "c": {"entry": ["enter_c"], "always": "d"},
            "d": {"entry": ["enter_d"], "always": "e"},
            "e": {"entry": ["enter_e"], "on": {"PING": "f"}},
            "f": {"entry": ["enter_f"]}
        }
    })");
    xstate::actor_system system(xactor::budgets{.fuel = 3});
    action_record record;
    record_actions(system, record);
    const xstate::actor_ref actor = system.create_actor(chain).value();
    BOOST_TEST(system.start(actor).value() == xstate::run_outcome::settled);

    BOOST_TEST(system.send(actor, named("GO")).value() == xstate::run_outcome::out_of_fuel);
    const std::vector<std::string> paid_for{"enter_a", "enter_b", "enter_c"};
    BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), paid_for.begin(), paid_for.end());
    BOOST_TEST_EQ(system.snapshot_of(actor)->value, parsed(R"("idle")"));

    BOOST_TEST(system.send(actor, named("PING")).value() == xstate::run_outcome::out_of_fuel);
    BOOST_TEST_EQ(record.types.size(), 3U);

    BOOST_TEST(system.resume().value() == xstate::run_outcome::settled);
    const std::vector<std::string> all{
        "enter_a", "enter_b", "enter_c", "enter_d", "enter_e", "enter_f",
    };
    BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), all.begin(), all.end());
    BOOST_TEST_EQ(system.snapshot_of(actor)->value, parsed(R"("f")"));
}

// A child that finishes while its parent is parked mid-macrostep has its done
// event wait until the parent's macrostep is finished.
void a_child_done_while_its_parent_is_parked_waits_for_the_parent_to_settle() {
    const xstate::machine worker = machine_of(R"({
        "initial": "working",
        "states": {
            "working": {"on": {"FINISH": "finished"}},
            "finished": {"type": "final"}
        }
    })");
    const xstate::machine parent = machine_of(R"({
        "initial": "idle",
        "invoke": {
            "src": "worker",
            "id": "kid",
            "onDone": {"actions": ["kid_done"]}
        },
        "states": {
            "idle": {"on": {"GO": "s1"}},
            "s1": {"entry": ["e1"], "always": "s2"},
            "s2": {"entry": ["e2"], "always": "s3"},
            "s3": {"entry": ["e3"], "always": "s4"},
            "s4": {"entry": ["e4"], "always": "s5"},
            "s5": {"entry": ["e5"], "always": "s6"},
            "s6": {"entry": ["e6"]}
        }
    })",
                                              with_actors({{"worker", worker}}));
    // The start costs exactly 5: the root's initial microstep, the kid's
    // construction, its initial microstep, its answer and its start. GO's
    // six microsteps pay five and park.
    xstate::actor_system system(xactor::budgets{.fuel = 5});
    action_record record;
    record_actions(system, record);
    const xstate::actor_ref root = started(system, parent);
    const xstate::actor_ref kid = child(system, root, "kid");

    BOOST_TEST(system.send(root, named("GO")).value() == xstate::run_outcome::out_of_fuel);
    // The host's FINISH waits while an actor is parked, so the parent's
    // macrostep settles before the child hears of it.
    BOOST_TEST(system.send(kid, named("FINISH")).value() == xstate::run_outcome::out_of_fuel);
    BOOST_TEST(system.status_of(kid).value() == xactor::status::active);
    const std::vector<std::string> before{"e1", "e2", "e3", "e4", "e5"};
    BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), before.begin(), before.end());

    BOOST_TEST(system.resume().value() == xstate::run_outcome::settled);
    BOOST_TEST(system.status_of(kid).value() == xactor::status::done);
    const std::vector<std::string> after{"e1", "e2", "e3", "e4", "e5", "e6", "kid_done"};
    BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), after.begin(), after.end());
}

xstate::raise_action raising_later(std::string type, std::uint64_t delay,
                                   std::optional<std::string> id = std::nullopt) {
    return xstate::raise_action{
        .event = making(std::move(type)),
        .id = std::move(id),
        .delay = xstate::delay_ref{delay},
    };
}

void after_fires_on_the_tick_that_reaches_it_and_not_before() {
    const xstate::machine waiting = machine_of(R"({
        "initial": "a",
        "states": {"a": {"after": {"100": "b"}}, "b": {}}
    })");
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, waiting);
    if (!BOOST_TEST(system.clock_tick(99).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("a")"));
    if (!BOOST_TEST(system.clock_tick(100).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("b")"));
}

void leaving_a_state_cancels_its_after() {
    const xstate::machine waiting = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {"after": {"100": "b"}, "on": {"LEAVE": "c"}},
            "b": {"entry": ["entered_b"]},
            "c": {"on": {"BACK": "a"}}
        }
    })");
    xstate::actor_system system(plenty);
    action_record record;
    record_actions(system, record);
    const xstate::actor_ref root = started(system, waiting);
    if (!BOOST_TEST(system.send(root, named("LEAVE")).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.clock_tick(500).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("c")"));
    BOOST_TEST(record.types.empty());
}

void a_delayed_send_reaches_the_child_at_its_deadline() {
    const xstate::machine listener = machine_of(
        R"({
            "initial": "idle",
            "states": {
                "idle": {"on": {"PING": "pinged"}},
                "pinged": {}
            }
        })");
    xstate::implementations registry = with_actors({{"listener", listener}});
    registry.actions.emplace("ping_later", xstate::send_to_action{
                                               .target = "kid",
                                               .event = making("PING"),
                                               .id = "later",
                                               .delay = xstate::delay_ref{std::uint64_t{50}},
                                           });
    registry.actions.emplace("forget", xstate::cancel_action{.id = "later"});
    const xstate::machine parent = machine_of(R"({
        "invoke": {"src": "listener", "id": "kid"},
        "on": {
            "GO": {"actions": ["ping_later"]},
            "FORGET": {"actions": ["forget"]}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    const xstate::actor_ref kid = child(system, root, "kid");
    if (!BOOST_TEST(system.clock_tick(10).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(root, named("GO")).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.clock_tick(59).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(kid)->value, parsed(R"("idle")"));
    if (!BOOST_TEST(system.clock_tick(60).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(kid)->value, parsed(R"("pinged")"));
}

void a_cancelled_delayed_send_never_arrives() {
    const xstate::machine listener = machine_of(
        R"({
            "initial": "idle",
            "states": {
                "idle": {"on": {"PING": "pinged"}},
                "pinged": {}
            }
        })");
    xstate::implementations registry = with_actors({{"listener", listener}});
    registry.actions.emplace("ping_later", xstate::send_to_action{
                                               .target = "kid",
                                               .event = making("PING"),
                                               .id = "later",
                                               .delay = xstate::delay_ref{std::uint64_t{50}},
                                           });
    registry.actions.emplace("forget", xstate::cancel_action{.id = "later"});
    const xstate::machine parent = machine_of(R"({
        "invoke": {"src": "listener", "id": "kid"},
        "on": {
            "GO": {"actions": ["ping_later"]},
            "FORGET": {"actions": ["forget"]}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    if (!BOOST_TEST(system.send(root, named("GO")).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(root, named("FORGET")).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.clock_tick(100).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(child(system, root, "kid"))->value, parsed(R"("idle")"));
}

void equal_deadlines_fire_in_arming_order() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("later_one", raising_later("ONE", 10));
    registry.actions.emplace("later_two", raising_later("TWO", 10));
    const xstate::machine timed = machine_of(R"({
        "entry": ["later_one", "later_two"],
        "on": {
            "ONE": {"actions": ["one"]},
            "TWO": {"actions": ["two"]}
        }
    })",
                                             std::move(registry));
    xstate::actor_system system(plenty);
    action_record record;
    record_actions(system, record);
    started(system, timed);
    if (!BOOST_TEST(system.clock_tick(10).has_value())) {
        return;
    }
    const std::vector<std::string> expected{"one", "two"};
    BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), expected.begin(), expected.end());
}

void a_stopped_actor_s_timers_never_fire() {
    const xstate::machine sleeper = machine_of(R"({
        "initial": "asleep",
        "states": {
            "asleep": {"after": {"100": "awake"}},
            "awake": {"entry": ["woke"]}
        }
    })");
    const xstate::machine parent = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {"invoke": {"src": "sleeper", "id": "kid"}, "on": {"LEAVE": "b"}},
            "b": {}
        }
    })",
                                              with_actors({{"sleeper", sleeper}}));
    xstate::actor_system system(plenty);
    action_record record;
    record_actions(system, record);
    const xstate::actor_ref root = started(system, parent);
    if (!BOOST_TEST(system.send(root, named("LEAVE")).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.clock_tick(200).has_value())) {
        return;
    }
    BOOST_TEST(record.types.empty());
}

// A timer armed while a tick runs, for a deadline the tick has reached,
// fires in the same tick (doc: #xstate-invariant-a7).
void a_timer_due_by_the_tick_that_armed_it_fires_in_that_tick() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("now", raising_later("NOW", 0));
    const xstate::machine chained = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {"after": {"100": "b"}},
            "b": {"entry": ["now"], "on": {"NOW": "c"}},
            "c": {}
        }
    })",
                                               std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, chained);
    if (!BOOST_TEST(system.clock_tick(100).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("c")"));
}

// XState arms a delayed sendTo to a child spawned later in the same entry in
// effect order, so a cancel after it in that entry cancels it.
void a_delayed_send_to_a_child_spawned_later_is_cancelled_by_a_later_cancel() {
    const xstate::machine listener = machine_of(
        R"({
            "initial": "idle",
            "states": {
                "idle": {"on": {"PING": "pinged"}},
                "pinged": {}
            }
        })");
    xstate::implementations registry = with_actors({{"listener", listener}});
    registry.actions.emplace("ping_later", xstate::send_to_action{
                                               .target = "kid",
                                               .event = making("PING"),
                                               .id = "later",
                                               .delay = xstate::delay_ref{std::uint64_t{0}},
                                           });
    registry.actions.emplace("forget", xstate::cancel_action{.id = "later"});
    const xstate::machine parent = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {"entry": ["ping_later", "forget"], "invoke": {"src": "listener", "id": "kid"}}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    if (!BOOST_TEST(system.clock_tick(10).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(child(system, root, "kid"))->value, parsed(R"("idle")"));
}

// A child asked to stop while it is parked finishes what it was doing
// first, on resume(), as XState's macrostep always runs to its end.
void a_parked_child_finishes_its_macrostep_before_it_stops() {
    const xstate::machine listener = machine_of(R"({
        "context": {"log": []},
        "on": {
            "BYE": {
                "actions": [
                    {"type": "push", "params": {"key": "log", "value": "bye"}}
                ]
            }
        }
    })");
    xstate::implementations registry = with_actors({{"listener", listener}});
    registry.actions.emplace("bye", sending("kid", "BYE"));
    const xstate::machine parent = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {
                "exit": ["bye"],
                "invoke": {"src": "listener", "id": "kid"},
                "on": {"LEAVE": "b"}
            },
            "b": {"always": "c"},
            "c": {"always": "d"},
            "d": {}
        }
    })",
                                              std::move(registry));
    // The start costs exactly 5, as a child's construction and start do;
    // LEAVE pays its three microsteps, BYE and the stop, so the kid parks
    // before BYE's microstep.
    xstate::actor_system system(xactor::budgets{.fuel = 5});
    const xstate::actor_ref root = started(system, parent);
    const xstate::actor_ref kid = child(system, root, "kid");
    BOOST_TEST(system.send(root, named("LEAVE")).value() == xstate::run_outcome::out_of_fuel);
    BOOST_TEST(system.resume().value() == xstate::run_outcome::settled);
    BOOST_TEST_EQ(system.snapshot_of(kid)->context, parsed(R"({"log": ["bye"]})"));
    BOOST_TEST(system.status_of(kid).value() == xactor::status::stopped);
}

// XState's clock fires due timers one at a time, so a cancel run by an
// earlier one stops a later one of the same tick.
void a_timer_cancelled_by_an_earlier_one_of_the_same_tick_never_fires() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("time_out", raising_later("TIMEOUT", 200, "timeout"));
    registry.actions.emplace("forget", xstate::cancel_action{.id = "timeout"});
    const xstate::machine timed = machine_of(R"({
        "initial": "waiting",
        "on": {"TIMEOUT": ".failed"},
        "states": {
            "waiting": {"entry": ["time_out"], "exit": ["forget"], "after": {"100": "ok"}},
            "ok": {},
            "failed": {}
        }
    })",
                                             std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, timed);
    if (!BOOST_TEST(system.clock_tick(300).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("ok")"));
}

// A tick the host brings while an actor is parked waits too, so a timer
// the parked macrostep cancels never fires.
void a_tick_waits_for_a_parked_actor_whose_macrostep_cancels_the_timer() {
    xstate::implementations registry = vocabulary::of_case({});
    add_raises(registry);
    const xstate::machine idle = machine_of(R"({
        "initial": "idle",
        "states": {
            "idle": {
                "after": {"100": "timeout"},
                "on": {
                    "ACTIVITY": {"target": "idle", "reenter": true, "actions": ["one", "two"]}
                }
            },
            "timeout": {}
        }
    })",
                                            std::move(registry));
    xstate::actor_system system(xactor::budgets{.fuel = 2});
    const xstate::actor_ref root = started(system, idle);
    if (!BOOST_TEST(system.clock_tick(60).has_value())) {
        return;
    }
    BOOST_TEST(system.send(root, named("ACTIVITY")).value() == xstate::run_outcome::out_of_fuel);
    BOOST_TEST(system.clock_tick(100).value() == xstate::run_outcome::out_of_fuel);
    BOOST_TEST(system.resume().value() == xstate::run_outcome::settled);
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("idle")"));
    if (!BOOST_TEST(system.clock_tick(160).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("timeout")"));
}

/**
 What a run did, actor by actor: each settled macrostep, custom action, emit
 and the end status.
*/
using run_trace = std::map<std::string, std::vector<std::string>>;

/** The ids of an actor and its ancestors, from the root. */
std::string path_of(const xstate::actor_system& system, xstate::actor_ref actor) {
    std::string path(system.id_of(actor));
    for (std::optional<xstate::actor_ref> up = system.parent_of(actor); up.has_value();
         up = system.parent_of(*up)) {
        std::string prefix(system.id_of(*up));
        prefix += '/';
        path.insert(0, prefix);
    }
    return path;
}

std::string_view status_name(xactor::status ended) {
    switch (ended) {
        case xactor::status::active: return "active";
        case xactor::status::done: return "done";
        case xactor::status::error: return "error";
        case xactor::status::stopped: return "stopped";
    }
    return "unknown";
}

/**
 A system that records into `trace`, and every actor it starts into `seen`. A failed check names
 `note`, the script and the budget its caller runs.
*/
template <class... Note>
void trace_into(xstate::actor_system& system, run_trace& trace,
                std::vector<xstate::actor_ref>& seen, const Note&... note) {
    const xstate::result<void> inspecting = system.inspect(xstate::inspector{
        .started = [&seen](xstate::actor_ref actor) { seen.push_back(actor); },
        .settled =
            [&system, &trace](xstate::actor_ref actor, const xstate::machine& /*logic*/,
                              const xstate::event& cause, const xstate::snapshot& settled) {
                trace[path_of(system, actor)].push_back(
                    cause.type + " -> " + boost::json::serialize(settled.value) + " " +
                    boost::json::serialize(settled.context));
            },
        .emitted =
            [&system, &trace](xstate::actor_ref actor, const xstate::event& emitted) {
                trace[path_of(system, actor)].push_back("emit " + emitted.type);
            },
    });
    require(noted(BOOST_TEST(inspecting.has_value()), note...));
    const xstate::result<void> acting =
        system.on_action([&system, &trace](xstate::actor_ref actor, const xstate::action& done) {
            trace[path_of(system, actor)].push_back("action " + done.type);
        });
    require(noted(BOOST_TEST(acting.has_value()), note...));
}

/** A host's script: the calls it makes on a system whose root it has started. */
using script = std::function<bool(xstate::actor_system&, xstate::actor_ref)>;

/**
 Runs `logic` and `then` on a system with `limits`, resuming after every call. A failed check
 names `note`.
*/
template <class... Note>
run_trace traced(const xstate::machine& logic, xactor::budgets limits, const script& then,
                 const Note&... note) {
    xstate::actor_system system(limits);
    run_trace trace;
    std::vector<xstate::actor_ref> seen;
    trace_into(system, trace, seen, note...);
    const xstate::actor_ref root = system.create_actor(logic).value();
    require(noted(BOOST_TEST(settles(system, system.start(root))), note...));
    require(noted(BOOST_TEST(then(system, root)), note...));
    for (const xstate::actor_ref actor : seen) {
        trace[path_of(system, actor)].push_back(
            "status " + std::string(status_name(system.status_of(actor).value())));
    }
    return trace;
}

std::vector<std::string> actors_of(const run_trace& trace) {
    std::vector<std::string> paths;
    for (const auto& [path, entries] : trace) {
        paths.push_back(path);
    }
    return paths;
}

/**
 Checks that `actual` shows each actor doing what `expected` shows it doing. A failed check names
 `note`, then the actor.
*/
template <class... Note>
void same_run(const run_trace& actual, const run_trace& expected, const Note&... note) {
    const std::vector<std::string> actual_actors = actors_of(actual);
    const std::vector<std::string> expected_actors = actors_of(expected);
    noted(BOOST_TEST_ALL_EQ(actual_actors.begin(), actual_actors.end(), expected_actors.begin(),
                            expected_actors.end()),
          note...);
    for (const auto& [path, entries] : expected) {
        const auto found = actual.find(path);
        require(noted(BOOST_TEST(found != actual.end()), note..., ", actor ", path));
        noted(BOOST_TEST_ALL_EQ(found->second.begin(), found->second.end(), entries.begin(),
                                entries.end()),
              note..., ", actor ", path);
    }
}

/**
 Checks that every budget from 1 to `most` leaves each actor doing what plenty of fuel does. A
 failed check names `note`, then the budget.
*/
template <class... Note>
void every_budget_does_what_plenty_does(const xstate::machine& logic, const script& then,
                                        std::uint32_t most, const Note&... note) {
    const run_trace expected = traced(logic, plenty, then, note...);
    for (std::uint32_t fuel = 1; fuel <= most; ++fuel) {
        same_run(traced(logic, xactor::budgets{.fuel = fuel}, then, note..., ", fuel ", fuel),
                 expected, note..., ", fuel ", fuel);
    }
}

xstate::send_to_action sending_later(std::string target, std::string type, std::uint64_t delay) {
    xstate::send_to_action sent = sending(std::move(target), std::move(type));
    sent.delay = xstate::delay_ref{delay};
    return sent;
}

// Whatever the budget, resuming until the system settles runs every
// microstep and effect once, in order (doc: #xstate-invariant-a3):
// a child's construction and start, the deferred sends and emits, a done
// report, an error report, a host's answer, a timer's relay, and a stop
// that reaches a grandchild.
void any_budget_resumed_to_the_end_does_what_plenty_of_fuel_does() {
    const xstate::machine grand = machine_of(R"({
        "context": {"log": []},
        "on": {
            "BYE": {
                "actions": [
                    {"type": "push", "params": {"key": "log", "value": "bye"}}
                ]
            }
        }
    })");
    xstate::implementations worker_registry = with_actors({{"grand", grand}});
    worker_registry.actions.emplace("tell_grand", sending("gk", "BYE"));
    const xstate::machine worker = machine_of(R"({
        "context": {"log": []},
        "initial": "counting",
        "invoke": {"src": "grand", "id": "gk"},
        "states": {
            "counting": {
                "on": {
                    "A": {
                        "actions": [
                            {"type": "push", "params": {"key": "log", "value": "a"}},
                            "counted"
                        ]
                    },
                    "B": {"target": "finished", "actions": ["tell_grand"]},
                    "BREAK": {"actions": ["fail"]},
                    "PING": {
                        "actions": [
                            {"type": "push", "params": {"key": "log", "value": "ping"}}
                        ]
                    },
                    "PONG": {
                        "actions": [
                            {"type": "push", "params": {"key": "log", "value": "pong"}}
                        ]
                    }
                }
            },
            "finished": {"type": "final", "entry": ["bye"]}
        }
    })",
                                              std::move(worker_registry));
    xstate::implementations registry = with_actors({{"worker", worker}}, with_host());
    registry.actions.emplace("send_a", sending("kid", "A"));
    registry.actions.emplace("send_b", sending("kid", "B"));
    registry.actions.emplace("send_break", sending("kid", "BREAK"));
    registry.actions.emplace("ping_later", sending_later("kid", "PING", 100));
    registry.actions.emplace("pong_later", sending_later("kid", "PONG", 100));
    registry.actions.emplace("shout", emitting("SHOUT"));
    const xstate::machine parent = machine_of(R"({
        "context": {},
        "initial": "idle",
        "states": {
            "idle": {"on": {"GO": "working", "FETCH": "fetching"}},
            "working": {
                "entry": ["started_work"],
                "invoke": {
                    "src": "worker",
                    "id": "kid",
                    "onDone": {"target": "idle", "actions": ["kid_done"]},
                    "onError": {"target": "idle", "actions": ["kid_failed"]}
                },
                "on": {
                    "SEND": {"actions": ["send_a", "send_a", "shout", "send_b"]},
                    "BREAK": {"actions": ["send_break"]},
                    "LATER": {"actions": ["ping_later"]},
                    "BOTH_LATER": {"actions": ["ping_later", "pong_later"]},
                    "LEAVE": "idle"
                }
            },
            "fetching": {
                "invoke": {
                    "src": "fetchUser",
                    "id": "fetch",
                    "onDone": {"target": "idle", "actions": ["fetched"]}
                }
            }
        }
    })",
                                              std::move(registry));
    const auto send = [](xstate::actor_system& system, xstate::actor_ref root,
                         std::string_view type) {
        return settles(system, system.send(root, named(type)));
    };
    const std::vector<std::pair<std::string_view, script>> scripts{
        {
            "a child counts what its parent sends, finishes and reports done",
            [send](xstate::actor_system& system, xstate::actor_ref root) {
                return send(system, root, "GO") && send(system, root, "SEND");
            },
        },
        {
            "a child fails and reports its error",
            [send](xstate::actor_system& system, xstate::actor_ref root) {
                return send(system, root, "GO") && send(system, root, "BREAK");
            },
        },
        {
            "a host actor's answer reaches its parent",
            [send](xstate::actor_system& system, xstate::actor_ref root) {
                if (!send(system, root, "FETCH")) {
                    return false;
                }
                const std::vector<xstate::host_request> requests = system.host_requests();
                return requests.size() == 1 &&
                       settles(system, system.resolve(requests.front(), parsed(R"({"id": 1})")));
            },
        },
        {
            "a delayed send is relayed when its timer fires",
            [send](xstate::actor_system& system, xstate::actor_ref root) {
                return send(system, root, "GO") && send(system, root, "LATER") &&
                       settles(system, system.clock_tick(100));
            },
        },
        {
            // A tick releases one timer at a time and stops at the first
            // actor that parks, so a relay never reaches a parked actor.
            "two delayed sends due on one tick are relayed in order",
            [send](xstate::actor_system& system, xstate::actor_ref root) {
                return send(system, root, "GO") && send(system, root, "BOTH_LATER") &&
                       settles(system, system.clock_tick(100));
            },
        },
        {
            "a child asked to stop asks its own child to stop",
            [send](xstate::actor_system& system, xstate::actor_ref root) {
                return send(system, root, "GO") && send(system, root, "LEAVE");
            },
        },
    };
    for (const auto& [name, then] : scripts) {
        every_budget_does_what_plenty_does(parent, then, 40, name);
    }
}

// A stopped actor's timers stop with it, the earlier of two armed under one
// id too, where XState's cancelAll cancels only the one its _scheduledEvents
// lists and the earlier still reaches the parent (doc: #differences-timers).
void a_stopped_actor_s_earlier_timer_under_an_id_never_fires() {
    xstate::implementations kid_registry = vocabulary::of_case({});
    xstate::send_parent_action ping = sending_up("PING");
    ping.id = "t";
    ping.delay = xstate::delay_ref{std::uint64_t{100}};
    xstate::send_parent_action pong = sending_up("PONG");
    pong.id = "t";
    pong.delay = xstate::delay_ref{std::uint64_t{200}};
    kid_registry.actions.emplace("ping_later", std::move(ping));
    kid_registry.actions.emplace("pong_later", std::move(pong));
    const xstate::machine kid =
        machine_of(R"({"entry": ["ping_later", "pong_later"]})", std::move(kid_registry));
    const xstate::machine parent = machine_of(R"({
        "context": {"log": []},
        "initial": "a",
        "states": {
            "a": {"invoke": {"src": "kid", "id": "k"}, "on": {"LEAVE": "b"}},
            "b": {}
        },
        "on": {
            "PING": {"actions": [{"type": "push", "params": {"key": "log", "value": "ping"}}]},
            "PONG": {"actions": [{"type": "push", "params": {"key": "log", "value": "pong"}}]}
        }
    })",
                                              with_actors({{"kid", kid}}));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    if (!BOOST_TEST(system.send(root, named("LEAVE")).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.clock_tick(300).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->context, parsed(R"({"log": []})"));
}

}  // namespace

int main() {
    a_parked_macrostep_settles_before_an_event_that_arrived_meanwhile();
    a_child_done_while_its_parent_is_parked_waits_for_the_parent_to_settle();
    after_fires_on_the_tick_that_reaches_it_and_not_before();
    leaving_a_state_cancels_its_after();
    a_delayed_send_reaches_the_child_at_its_deadline();
    a_cancelled_delayed_send_never_arrives();
    equal_deadlines_fire_in_arming_order();
    a_stopped_actor_s_timers_never_fire();
    a_timer_due_by_the_tick_that_armed_it_fires_in_that_tick();
    a_delayed_send_to_a_child_spawned_later_is_cancelled_by_a_later_cancel();
    a_parked_child_finishes_its_macrostep_before_it_stops();
    a_timer_cancelled_by_an_earlier_one_of_the_same_tick_never_fires();
    a_tick_waits_for_a_parked_actor_whose_macrostep_cancels_the_timer();
    any_budget_resumed_to_the_end_does_what_plenty_of_fuel_does();
    a_stopped_actor_s_earlier_timer_under_an_id_never_fires();
    return boost::report_errors();
}
