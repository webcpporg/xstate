// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Tests the macrostep cursor's own behaviour
 (doc: #xstate-invariant-6 to #xstate-invariant-8 and #xstate-invariant-11):
 one microstep a call, the settling microstep marked and no other, a copy
 that resumes where it stopped, a cycle the library never stops, and a
 failure that settles in error. XState has no test of these.
*/

#include <webcpp/xstate.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/json.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "require.hpp"
#include "vocabulary.hpp"

namespace xstate = webcpp::xstate;
namespace vocabulary = webcpp::test::xstate_vocabulary;

using webcpp::test::noted;
using webcpp::test::require;

namespace {

// GO runs 8 microsteps: idle to a, a to b (whose entry raises PING), PING to
// c, then c and loop twice while count reaches 2, then c to c2. SPIN enters
// a cycle of eventless transitions that never settles.
constexpr std::string_view cursor_case = R"({
    "machine": {
        "id": "m",
        "initial": "idle",
        "context": {"count": 0},
        "states": {
            "idle": {
                "on": {
                    "GO": {"target": "a", "actions": ["go"]},
                    "BREAK": {"target": "a", "actions": ["go", "fail"]},
                    "BREAK_GUARD": {"target": "a", "guard": "fail"}
                }
            },
            "a": {
                "always": {"target": "b", "actions": ["a_to_b"]}
            },
            "b": {"entry": ["ping"], "on": {"PING": "c"}},
            "c": {
                "always": [
                    {
                        "target": "c2",
                        "guard": {
                            "type": "context_at_least",
                            "params": {"key": "count", "value": 2}
                        }
                    },
                    {
                        "target": "loop",
                        "actions": [
                            {"type": "increment", "params": {"key": "count"}}
                        ]
                    }
                ]
            },
            "loop": {"always": "c"},
            "c2": {"on": {"SPIN": "spin1"}},
            "spin1": {"always": "spin2"},
            "spin2": {"always": "spin1"}
        }
    },
    "actions": {
        "ping": {"kind": "raise", "event": {"type": "PING"}}
    }
})";

/** The cursor machine, started. */
struct started {
    xstate::machine machine;
    xstate::snapshot initial;
};

started start() {
    boost::system::error_code failed;
    const boost::json::value the_case = boost::json::parse(cursor_case, failed);
    require(BOOST_TEST(!failed));
    xstate::result<xstate::machine> created =
        xstate::create_machine(the_case.at("machine"), vocabulary::of_case(the_case.get_object()));
    require(BOOST_TEST(created.has_value()));
    xstate::snapshot initial = xstate::get_initial_snapshot(*created);
    return {.machine = std::move(*created), .initial = std::move(initial)};
}

xstate::event named(std::string_view type) {
    return {.type = std::string(type), .payload = {}};
}

/** Runs `cursor` to its end, returning which microsteps were marked settled. */
std::vector<bool> run_to_end(xstate::macrostep& cursor) {
    std::vector<bool> marks;
    while (!cursor.done()) {
        marks.push_back(cursor.next().settled);
    }
    return marks;
}

/**
 Whether two settled macrosteps are the same: snapshot, microsteps, actions in order. A failed
 check names `stopped`, the row of the case that calls it.
*/
void check_same(const xstate::macrostep_result& actual, const xstate::macrostep_result& expected,
                std::size_t stopped) {
    noted(BOOST_TEST_EQ(actual.snapshot.value, expected.snapshot.value), "for stopped = ", stopped);
    noted(BOOST_TEST_EQ(actual.snapshot.context, expected.snapshot.context),
          "for stopped = ", stopped);
    noted(BOOST_TEST_ALL_EQ(actual.snapshot.nodes.begin(), actual.snapshot.nodes.end(),
                            expected.snapshot.nodes.begin(), expected.snapshot.nodes.end()),
          "for stopped = ", stopped);
    noted(BOOST_TEST_EQ(actual.microsteps, expected.microsteps), "for stopped = ", stopped);
    require(noted(BOOST_TEST_EQ(actual.actions.size(), expected.actions.size()),
                  "for stopped = ", stopped));
    for (std::size_t index = 0; index < actual.actions.size(); ++index) {
        noted(BOOST_TEST_EQ(actual.actions[index].type, expected.actions[index].type),
              "for stopped = ", stopped, ", action ", index);
        noted(BOOST_TEST_EQ(actual.actions[index].params, expected.actions[index].params),
              "for stopped = ", stopped, ", action ", index);
    }
}

const std::array<std::size_t, 5> stopping_points{{1, 2, 3, 5, 7}};

const std::array<std::string_view, 2> failing_events{{"BREAK", "BREAK_GUARD"}};

void the_macrostep_settles_on_the_microstep_that_closes_it_and_on_no_other() {
    const started machine = start();
    xstate::macrostep cursor = xstate::begin(machine.machine, machine.initial, named("GO"));
    const std::vector<bool> marks = run_to_end(cursor);
    if (!BOOST_TEST_EQ(marks.size(), 8U)) {
        return;
    }
    for (std::size_t index = 0; index < marks.size(); ++index) {
        noted(BOOST_TEST_EQ(marks[index], index + 1 == marks.size()), "microstep ", index + 1);
    }
    BOOST_TEST_EQ(cursor.result().microsteps, 8U);
    BOOST_TEST_EQ(cursor.result().snapshot.value, boost::json::value("c2"));
}

void an_event_that_selects_no_transition_settles_at_once_changing_nothing() {
    const started machine = start();
    xstate::macrostep cursor = xstate::begin(machine.machine, machine.initial, named("NOTHING"));
    const std::vector<bool> marks = run_to_end(cursor);
    if (!BOOST_TEST_EQ(marks.size(), 1U)) {
        return;
    }
    BOOST_TEST(marks.front());
    const xstate::snapshot& settled = cursor.result().snapshot;
    BOOST_TEST_EQ(settled.value, machine.initial.value);
    BOOST_TEST_EQ(settled.context, machine.initial.context);
    BOOST_TEST_ALL_EQ(settled.nodes.begin(), settled.nodes.end(), machine.initial.nodes.begin(),
                      machine.initial.nodes.end());
    BOOST_TEST(cursor.result().actions.empty());
}

// Each data case of the old suite is a function of one row, called for every row of the same
// data, in order: a failed check names its row on the line after it, and a failed required
// check ends its row, as Boost.Test ended the row's case.

void a_copy_of_a_stopped_cursor_settles_where_a_straight_run_settles(std::size_t stopped) {
    const started machine = start();
    xstate::macrostep straight = xstate::begin(machine.machine, machine.initial, named("GO"));
    run_to_end(straight);

    xstate::macrostep original = xstate::begin(machine.machine, machine.initial, named("GO"));
    for (std::size_t step = 0; step < stopped && !original.done(); ++step) {
        original.next();
    }
    xstate::macrostep copy = original;
    run_to_end(copy);
    run_to_end(original);
    check_same(copy.result(), straight.result(), stopped);
    check_same(original.result(), straight.result(), stopped);
}

void a_copy_of_a_stopped_cursor_settles_where_a_straight_run_settles() {
    for (const std::size_t stopped : stopping_points) {
        a_copy_of_a_stopped_cursor_settles_where_a_straight_run_settles(stopped);
    }
}

void an_eventless_cycle_never_settles_and_only_the_caller_stops_it() {
    const started machine = start();
    xstate::macrostep go = xstate::begin(machine.machine, machine.initial, named("GO"));
    run_to_end(go);
    xstate::macrostep spin = xstate::begin(machine.machine, go.result().snapshot, named("SPIN"));
    for (int step = 0; step < 1000; ++step) {
        if (!noted(BOOST_TEST(!spin.next().settled), "settled after ", step + 1, " microsteps")) {
            return;
        }
    }
    BOOST_TEST(!spin.done());
}

void a_failing_implementation_settles_in_error_with_the_snapshot_it_began_from(
    std::string_view event_type) {
    const started machine = start();
    xstate::macrostep cursor = xstate::begin(machine.machine, machine.initial, named(event_type));
    noted(BOOST_TEST(cursor.next().settled), "for ", event_type);
    noted(BOOST_TEST(cursor.done()), "for ", event_type);
    const xstate::snapshot& settled = cursor.result().snapshot;
    noted(BOOST_TEST(settled.status == xstate::status::error), "for ", event_type);
    noted(BOOST_TEST_EQ(settled.error.message(), "implementation_failed"), "for ", event_type);
    noted(BOOST_TEST_EQ(settled.value, machine.initial.value), "for ", event_type);
    noted(BOOST_TEST_EQ(settled.context, machine.initial.context), "for ", event_type);
    noted(BOOST_TEST_ALL_EQ(settled.nodes.begin(), settled.nodes.end(),
                            machine.initial.nodes.begin(), machine.initial.nodes.end()),
          "for ", event_type);
}

void a_failing_implementation_settles_in_error_with_the_snapshot_it_began_from() {
    for (const std::string_view event_type : failing_events) {
        a_failing_implementation_settles_in_error_with_the_snapshot_it_began_from(event_type);
    }
}

// A microstep that fails reports no actions, as XState's pure functions
// throw; what it resolved before the failure, which a running actor of
// XState has run in part, is apart, and the result holds none of it.
void a_failing_microstep_reports_what_it_resolved_apart_from_its_actions() {
    const started machine = start();
    xstate::macrostep cursor = xstate::begin(machine.machine, machine.initial, named("BREAK"));
    const xstate::progress made = cursor.next();
    BOOST_TEST(made.settled);
    BOOST_TEST(made.step.actions.empty());
    if (!BOOST_TEST_EQ(made.resolved_before_failure.size(), 1U)) {
        return;
    }
    BOOST_TEST_EQ(made.resolved_before_failure.front().type, "go");
    BOOST_TEST(cursor.result().actions.empty());
}

/** A machine that runs `action` on GO, from its one state. */
xstate::machine acting_on_go(std::string_view action, xstate::implementations registry) {
    const boost::json::value config =
        boost::json::parse(R"({"initial": "a", "states": {"a": {"on": {"GO": {"actions": [")" +
                           std::string(action) + R"("]}}}}})");
    xstate::result<xstate::machine> created = xstate::create_machine(config, std::move(registry));
    require(BOOST_TEST(created.has_value()));
    return std::move(*created);
}

/**
 A machine that GO takes to its state b, whose eventless transition targets
 nothing and runs `action`: a spawn of the child kid, a stop of it, or a
 custom action.
*/
xstate::machine acting_without_event(std::string_view action) {
    xstate::implementations registry;
    registry.actors.emplace("child", xstate::host_actor{});
    registry.actions.emplace("spawn", xstate::spawn_child_action{
                                          .src = "child",
                                          .id = "kid",
                                          .system_id = std::nullopt,
                                          .input = std::nullopt,
                                      });
    registry.actions.emplace("stop", xstate::stop_child_action{.id = "kid"});
    const boost::json::value config = boost::json::parse(
        R"({"initial": "a", "states": {"a": {"on": {"GO": "b"}}, "b": {"always": {"actions": [")" +
        std::string(action) + R"("]}}}})");
    xstate::result<xstate::machine> created = xstate::create_machine(config, std::move(registry));
    require(BOOST_TEST(created.has_value()));
    return std::move(*created);
}

const std::array<std::string_view, 2> child_actions{{"spawn", "stop"}};

// A spawn or a stop changes the snapshot's children, as XState's
// resolveSpawn and resolveStop return a new snapshot, even for a stop of a
// child that is not there; so after an eventless microstep whose only effect
// is one, the eventless transitions are selected again, and one that keeps
// spawning or stopping never settles (doc: #xstate-invariant-9).
void an_eventless_microstep_that_only_spawns_or_stops_selects_again(std::string_view action) {
    const xstate::machine machine = acting_without_event(action);
    xstate::macrostep cursor =
        xstate::begin(machine, xstate::get_initial_snapshot(machine), named("GO"));
    for (int step = 0; step < 100; ++step) {
        if (!noted(BOOST_TEST(!cursor.next().settled), "for ", action, ": settled after ", step + 1,
                   " microsteps")) {
            return;
        }
    }
    noted(BOOST_TEST(!cursor.done()), "for ", action);
}

void an_eventless_microstep_that_only_spawns_or_stops_selects_again() {
    for (const std::string_view action : child_actions) {
        an_eventless_microstep_that_only_spawns_or_stops_selects_again(action);
    }
}

// An eventless microstep whose only effect is a custom action changes
// nothing, so the eventless transitions are not selected again after it and
// the macrostep settles there (doc: #xstate-invariant-9).
void an_eventless_microstep_that_changes_nothing_settles_the_macrostep() {
    const xstate::machine machine = acting_without_event("tick");
    xstate::macrostep cursor =
        xstate::begin(machine, xstate::get_initial_snapshot(machine), named("GO"));
    const std::vector<bool> marks = run_to_end(cursor);
    if (!BOOST_TEST_EQ(marks.size(), 2U)) {
        return;
    }
    BOOST_TEST(marks.back());
    BOOST_TEST_EQ(cursor.result().actions.size(), 1U);
}

// XState names a systemId with a function of its system, which JSON cannot
// hold, so the oracle has no case of this.
void a_send_to_a_system_id_is_returned_for_its_caller_to_resolve() {
    xstate::implementations registry;
    registry.actions.emplace(
        "notify", xstate::send_to_action{
                      .target = "#system:notifier",
                      .event = [](const xstate::action_args&) -> xstate::result<xstate::event> {
                          return xstate::event{.type = "NOTE", .payload = {}};
                      },
                      .id = std::nullopt,
                      .delay = std::nullopt,
                  });
    const xstate::machine machine = acting_on_go("notify", std::move(registry));
    const auto [next, actions] =
        xstate::transition(machine, xstate::get_initial_snapshot(machine), named("GO"));
    BOOST_TEST(next.status == xstate::status::active);
    if (!BOOST_TEST_EQ(actions.size(), 1U)) {
        return;
    }
    BOOST_TEST_EQ(actions.front().type, "xstate.sendTo");
    BOOST_TEST_EQ(
        actions.front().params,
        boost::json::parse(R"({"targetId": "#system:notifier", "event": {"type": "NOTE"}})"));
}

void a_spawn_whose_input_fails_settles_in_error_without_the_child() {
    xstate::implementations registry;
    registry.actors.emplace("child", xstate::host_actor{});
    registry.actions.emplace(
        "spawn",
        xstate::spawn_child_action{
            .src = "child",
            .id = "kid",
            .system_id = std::nullopt,
            .input = [](const xstate::action_args&) -> xstate::result<boost::json::value> {
                return xstate::failure<boost::json::value>(xstate::errc::implementation_failed);
            },
        });
    const xstate::machine machine = acting_on_go("spawn", std::move(registry));
    const xstate::snapshot settled =
        xstate::get_next_snapshot(machine, xstate::get_initial_snapshot(machine), named("GO"));
    BOOST_TEST(settled.status == xstate::status::error);
    BOOST_TEST_EQ(settled.error.message(), "implementation_failed");
    BOOST_TEST(settled.children.empty());
}

// A sendTo to an invoke of the state being entered is bound where XState's
// retryResolveSendTo binds it, once that state's entry actions, spawns and
// initial actions have resolved: `bound_at` counts its microstep's actions
// before that point. No other action has one.
void a_send_to_an_invoke_of_the_entered_state_is_bound_after_that_state_s_actions() {
    xstate::implementations registry;
    registry.actors.emplace("worker", xstate::host_actor{});
    registry.actions.emplace(
        "ping_n", xstate::send_to_action{
                      .target = "n",
                      .event = [](const xstate::action_args&) -> xstate::result<xstate::event> {
                          return xstate::event{.type = "PING", .payload = {}};
                      },
                      .id = std::nullopt,
                      .delay = std::nullopt,
                  });
    const xstate::result<xstate::machine> machine = xstate::create_machine(boost::json::parse(R"({
        "initial": "a",
        "states": {
            "a": {"on": {"GO": "b"}},
            "b": {
                "entry": ["ping_n", "hello"],
                "invoke": {"id": "n", "src": "worker"},
                "initial": "b1",
                "states": {"b1": {"entry": ["later"]}}
            }
        }
    })"),
                                                                           std::move(registry));
    if (!BOOST_TEST(machine.has_value())) {
        return;
    }
    const auto [next, actions] =
        xstate::transition(*machine, xstate::get_initial_snapshot(*machine), named("GO"));
    if (!BOOST_TEST_EQ(actions.size(), 4U)) {
        return;
    }
    BOOST_TEST_EQ(actions[0].type, "xstate.sendTo");
    BOOST_TEST(actions[0].bound_at == std::optional<std::size_t>(3));
    for (std::size_t index = 1; index < actions.size(); ++index) {
        noted(BOOST_TEST(!actions[index].bound_at.has_value()), "action ", actions[index].type);
    }
}

// The clock schedules a delayed raise the library resolved, and leaves a
// custom action alone even when the config names it xstate.raise, as
// XState runs no custom action itself.
void the_clock_schedules_only_a_built_in_delayed_raise() {
    const boost::json::value params = boost::json::parse(R"({
        "event": {"type": "nudge"},
        "delay": 10
    })");
    xstate::simulated_clock clock;
    clock.apply(xstate::action{.type = "xstate.raise", .params = params, .builtin = false});
    clock.increment(10);
    BOOST_TEST(!clock.pop_due().has_value());
    clock.apply(xstate::action{.type = "xstate.raise", .params = params, .builtin = true});
    clock.increment(10);
    const std::optional<xstate::event> due = clock.pop_due();
    if (!BOOST_TEST(due.has_value())) {
        return;
    }
    BOOST_TEST_EQ(due->type, "nudge");
}

}  // namespace

int main() {
    the_macrostep_settles_on_the_microstep_that_closes_it_and_on_no_other();
    an_event_that_selects_no_transition_settles_at_once_changing_nothing();
    a_copy_of_a_stopped_cursor_settles_where_a_straight_run_settles();
    an_eventless_cycle_never_settles_and_only_the_caller_stops_it();
    a_failing_implementation_settles_in_error_with_the_snapshot_it_began_from();
    a_failing_microstep_reports_what_it_resolved_apart_from_its_actions();
    an_eventless_microstep_that_only_spawns_or_stops_selects_again();
    an_eventless_microstep_that_changes_nothing_settles_the_macrostep();
    a_send_to_a_system_id_is_returned_for_its_caller_to_resolve();
    a_spawn_whose_input_fails_settles_in_error_without_the_child();
    a_send_to_an_invoke_of_the_entered_state_is_bound_after_that_state_s_actions();
    the_clock_schedules_only_a_built_in_delayed_raise();
    return boost::report_errors();
}
