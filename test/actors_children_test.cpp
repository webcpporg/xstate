// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Tests the children xstate's actors invoke and spawn (doc: #xstate-on-xactor):
 a child's output and error reaching its parent, its stop when its state is
 left, the order the children of a start start in, and the children of a
 macrostep that fails.

 Tip: XState's own actor tests are ported as cases against the oracle
 (actors_cases_test.cpp); this file and the other actors_*_test.cpp cover
 what the oracle cannot see.
*/

#include <webcpp/xstate/actors.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/json.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "actor_helpers.hpp"

namespace xstate = webcpp::xstate;
namespace xactor = webcpp::xactor;

using webcpp::test::xstate_actors::action_record;
using webcpp::test::xstate_actors::child;
using webcpp::test::xstate_actors::machine_of;
using webcpp::test::xstate_actors::named;
using webcpp::test::xstate_actors::parsed;
using webcpp::test::xstate_actors::plenty;
using webcpp::test::xstate_actors::record_actions;
using webcpp::test::xstate_actors::sending;
using webcpp::test::xstate_actors::started;
using webcpp::test::xstate_actors::with_actors;

namespace {

void an_invoked_child_runs_and_its_on_done_takes_its_output() {
    const xstate::machine worker = machine_of(R"({
        "id": "worker",
        "initial": "working",
        "output": {"answer": 42},
        "states": {
            "working": {"on": {"FINISH": "finished"}},
            "finished": {"type": "final"}
        }
    })");
    const xstate::machine parent = machine_of(R"({
        "id": "parent",
        "context": {},
        "initial": "loading",
        "states": {
            "loading": {
                "invoke": {
                    "src": "worker",
                    "id": "kid",
                    "onDone": {
                        "target": "loaded",
                        "actions": [
                            {
                                "type": "set_from_event",
                                "params": {"key": "result", "from": "output"}
                            }
                        ]
                    }
                }
            },
            "loaded": {}
        }
    })",
                                              with_actors({{"worker", worker}}));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    const xstate::actor_ref kid = child(system, root, "kid");
    BOOST_TEST_EQ(system.snapshot_of(kid)->value, parsed(R"("working")"));

    if (!BOOST_TEST(system.send(kid, named("FINISH")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(kid).value() == xactor::status::done);
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("loaded")"));
    BOOST_TEST_EQ(system.snapshot_of(root)->context, parsed(R"({"result": {"answer": 42}})"));
}

void a_child_s_error_reaches_on_error_and_unhandled_fails_the_parent() {
    const xstate::machine failing = machine_of(R"({
        "context": {},
        "on": {"BREAK": {"actions": ["fail"]}}
    })");
    const xstate::machine handling = machine_of(R"({
        "context": {},
        "initial": "running",
        "states": {
            "running": {
                "invoke": {
                    "src": "failing",
                    "id": "kid",
                    "onError": {
                        "target": "failed",
                        "actions": [
                            {
                                "type": "set_from_event",
                                "params": {"key": "reason", "from": "error"}
                            }
                        ]
                    }
                }
            },
            "failed": {}
        }
    })",
                                                with_actors({{"failing", failing}}));
    const xstate::machine careless = machine_of(R"({"invoke": {"src": "failing", "id": "kid"}})",
                                                with_actors({{"failing", failing}}));
    xstate::actor_system system(plenty);

    const xstate::actor_ref handler = started(system, handling);
    if (!BOOST_TEST(system.send(child(system, handler, "kid"), named("BREAK")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(handler)->value, parsed(R"("failed")"));
    BOOST_TEST_EQ(system.snapshot_of(handler)->context,
                  parsed(R"({"reason": "implementation_failed"})"));

    const xstate::actor_ref other = started(system, careless);
    if (!BOOST_TEST(system.send(child(system, other, "kid"), named("BREAK")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(other).value() == xactor::status::error);
    BOOST_TEST_EQ(system.snapshot_of(other)->error.message(), "actor_failed");
    BOOST_TEST_EQ(system.snapshot_of(other)->error_value.value(),
                  parsed(R"("implementation_failed")"));
}

void leaving_the_invoking_state_stops_the_child_and_its_own_children() {
    const xstate::machine grand = machine_of(R"({"id": "grand"})");
    const xstate::machine middle =
        machine_of(R"({"invoke": {"src": "grand", "id": "gk"}})", with_actors({{"grand", grand}}));
    const xstate::machine parent = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {"invoke": {"src": "middle", "id": "kid"}, "on": {"LEAVE": "b"}},
            "b": {}
        }
    })",
                                              with_actors({{"middle", middle}}));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    const xstate::actor_ref kid = child(system, root, "kid");
    const xstate::actor_ref grandchild = child(system, kid, "gk");
    BOOST_TEST(system.status_of(grandchild).value() == xactor::status::active);

    if (!BOOST_TEST(system.send(root, named("LEAVE")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(kid).value() == xactor::status::stopped);
    BOOST_TEST(system.status_of(grandchild).value() == xactor::status::stopped);
    BOOST_TEST(!system.child_of(root, "kid").has_value());
}

// An action of the config the implementations do not hold is a custom
// action, whatever its name; only what the library resolved is a built-in.
void a_config_action_named_like_a_built_in_is_a_custom_action() {
    xstate::implementations registry = with_actors({{"listener", machine_of(R"({})")}});
    const xstate::machine sneaky = machine_of(R"({
        "entry": [
            {
                "type": "xstate.spawnChild",
                "params": {"id": "c", "src": "listener", "systemId": 7}
            }
        ]
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    action_record record;
    record_actions(system, record);
    const xstate::actor_ref root = started(system, sneaky);
    BOOST_TEST(system.status_of(root).value() == xactor::status::active);
    BOOST_TEST(!system.child_of(root, "c").has_value());
    const std::vector<std::string> expected{"xstate.spawnChild"};
    BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), expected.begin(), expected.end());
}

// XState stops a child the same macrostep spawned before it starts: its
// start is skipped and the event sent to it is dropped.
void a_child_spawned_and_stopped_in_one_macrostep_never_starts() {
    const xstate::machine listener = machine_of(
        R"({
            "entry": ["child_started"],
            "on": {"PING": {"actions": ["child_pinged"]}}
        })");
    xstate::implementations registry = with_actors({{"listener", listener}});
    registry.actions.emplace("ping", sending("c", "PING"));
    const xstate::machine parent = machine_of(R"({
        "initial": "idle",
        "states": {
            "idle": {"on": {"GO": "a"}},
            "a": {"entry": ["ping"], "invoke": {"src": "listener", "id": "c"}, "always": "b"},
            "b": {}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    action_record record;
    record_actions(system, record);
    const xstate::actor_ref root = started(system, parent);
    if (!BOOST_TEST(system.send(root, named("GO")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("b")"));
    BOOST_TEST(record.types.empty());
}

// A stopped child's own children handle what it sent them before they stop,
// as XState's nested stops let them.
void a_stopped_child_s_children_handle_what_it_sent_them_first() {
    const xstate::machine leaf = machine_of(R"({
        "context": {"log": []},
        "on": {
            "BYE": {
                "actions": [
                    {"type": "push", "params": {"key": "log", "value": "bye"}}
                ]
            }
        }
    })");
    xstate::implementations middle_registry = with_actors({{"leaf", leaf}});
    middle_registry.actions.emplace("pass_on", sending("gk", "BYE"));
    const xstate::machine middle = machine_of(
        R"({
            "invoke": {"src": "leaf", "id": "gk"},
            "on": {"BYE": {"actions": ["pass_on"]}}
        })",
        std::move(middle_registry));
    xstate::implementations registry = with_actors({{"middle", middle}});
    registry.actions.emplace("bye", sending("kid", "BYE"));
    const xstate::machine parent = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {
                "exit": ["bye"],
                "invoke": {"src": "middle", "id": "kid"},
                "on": {"LEAVE": "b"}
            },
            "b": {}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    const xstate::actor_ref kid = child(system, root, "kid");
    const xstate::actor_ref gk = child(system, kid, "gk");
    if (!BOOST_TEST(system.send(root, named("LEAVE")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(gk)->context, parsed(R"({"log": ["bye"]})"));
    BOOST_TEST(system.status_of(kid).value() == xactor::status::stopped);
    BOOST_TEST(system.status_of(gk).value() == xactor::status::stopped);
}

// A failed child ignores its parent's stop, as XState's _stop does once an
// actor has stopped, so its own children keep running.
void a_failed_child_s_children_keep_running_when_its_parent_stops_it() {
    const xstate::machine grand = machine_of(R"({
        "initial": "idle",
        "states": {
            "idle": {"on": {"PING": "pinged"}},
            "pinged": {}
        }
    })");
    const xstate::machine middle = machine_of(R"({
        "context": {},
        "invoke": {"src": "grand", "id": "gk"},
        "on": {"BREAK": {"actions": ["fail"]}}
    })",
                                              with_actors({{"grand", grand}}));
    const xstate::machine parent = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {"invoke": {"src": "middle", "id": "kid", "onError": "b"}},
            "b": {}
        }
    })",
                                              with_actors({{"middle", middle}}));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    const xstate::actor_ref kid = child(system, root, "kid");
    const xstate::actor_ref grandchild = child(system, kid, "gk");

    if (!BOOST_TEST(system.send(kid, named("BREAK")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("b")"));
    BOOST_TEST(system.status_of(kid).value() == xactor::status::error);
    BOOST_TEST(system.status_of(grandchild).value() == xactor::status::active);
    if (!BOOST_TEST(system.send(grandchild, named("PING")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(grandchild)->value, parsed(R"("pinged")"));
}

// An actor's start starts its initial children in the order its snapshot
// lists them, and each actor's own initial actions keep their order, as
// XState's start() does (StateMachine.start starts snapshot.children, then
// update runs the deferred actions). Across actors the order differs
// (doc: #differences-interleaving): XState runs a child's start nested, so its
// actions come first, "c: cc", "i: cc", "x:0: a", "x:0: b"; here a child
// handles its start after its parent's turn.
void a_start_starts_the_initial_children_in_order_and_keeps_each_actors_actions_in_order() {
    xstate::implementations registry = with_actors({{"child", machine_of(R"({"entry": ["cc"]})")}});
    registry.actions.emplace("spawn_c", xstate::spawn_child_action{
                                            .src = "child",
                                            .id = "c",
                                            .system_id = std::nullopt,
                                            .input = std::nullopt,
                                        });
    const xstate::machine root_machine = machine_of(R"({
        "initial": "s",
        "states": {
            "s": {"entry": ["a", "spawn_c", "b"], "invoke": {"src": "child", "id": "i"}}
        }
    })",
                                                    std::move(registry));
    xstate::actor_system system(plenty);
    std::vector<std::string> handed;
    const xstate::result<void> acting =
        system.on_action([&system, &handed](xstate::actor_ref actor, const xstate::action& done) {
            handed.push_back(std::string(system.id_of(actor)) + ": " + done.type);
        });
    if (!BOOST_TEST(acting.has_value())) {
        return;
    }
    static_cast<void>(started(system, root_machine));
    const std::vector<std::string> expected{"x:0: a", "x:0: b", "c: cc", "i: cc"};
    BOOST_TEST_ALL_EQ(handed.begin(), handed.end(), expected.begin(), expected.end());
}

// A deferred effect that fails fails the actor with the snapshot its
// macrostep reached, and the children that macrostep created stop, as when
// its resolution fails (doc: #xstate-invariant-a9). Here the start of a child
// born done fails, its systemId taken meanwhile (doc: #xstate-invariant-a8).
void a_deferred_effect_that_fails_stops_the_children_its_macrostep_created() {
    const xstate::machine ended = machine_of(R"({
        "initial": "f",
        "states": {"f": {"type": "final"}}
    })");
    xstate::implementations registry = with_actors({
        {"ended", ended},
        {"worker", machine_of(R"({})")},
    });
    registry.actions.emplace("spawn_ended", xstate::spawn_child_action{
                                                .src = "ended",
                                                .id = "a",
                                                .system_id = "x",
                                                .input = std::nullopt,
                                            });
    registry.actions.emplace("spawn_x", xstate::spawn_child_action{
                                            .src = "worker",
                                            .id = "b",
                                            .system_id = "x",
                                            .input = std::nullopt,
                                        });
    registry.actions.emplace("spawn_w", xstate::spawn_child_action{
                                            .src = "worker",
                                            .id = "w",
                                            .system_id = "w",
                                            .input = std::nullopt,
                                        });
    const xstate::machine root_machine = machine_of(
        R"({
            "context": {"n": 0},
            "on": {
                "GO": {
                    "actions": [
                        {"type": "increment", "params": {"key": "n"}},
                        "spawn_ended",
                        "spawn_x",
                        "spawn_w"
                    ]
                }
            }
        })",
        std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, root_machine);
    if (!BOOST_TEST(system.send(root, named("GO")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(root).value() == xactor::status::error);
    BOOST_TEST_EQ(system.snapshot_of(root)->error.message(), "system_id_taken");
    BOOST_TEST_EQ(system.snapshot_of(root)->context, parsed(R"({"n": 1})"));
    BOOST_TEST(!system.child_of(root, "w").has_value());
    BOOST_TEST(!system.get("w").has_value());
    BOOST_TEST(!system.get("x").has_value());
}

void a_started_child_a_failing_macrostep_stopped_is_still_its_parent_s() {
    xstate::implementations registry = with_actors({{"worker", machine_of(R"({})")}});
    registry.actions.emplace("claim_app", xstate::spawn_child_action{
                                              .src = "worker",
                                              .id = "x",
                                              .system_id = "app",
                                              .input = std::nullopt,
                                          });
    const xstate::machine root_machine = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {
                "invoke": {"src": "worker", "id": "kid", "systemId": "k"},
                "on": {
                    "GO": {"target": "b", "actions": ["claim_app"]}
                }
            },
            "b": {}
        }
    })",
                                                    std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root =
        started(system, root_machine, {.input = nullptr, .id = std::nullopt, .system_id = "app"});
    const xstate::actor_ref kid = child(system, root, "kid");
    if (!BOOST_TEST(system.send(root, named("GO")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->error.message(), "system_id_taken");
    BOOST_TEST(system.status_of(kid).value() == xactor::status::active);
    BOOST_TEST(system.child_of(root, "kid") == std::optional(kid));
}

}  // namespace

int main() {
    an_invoked_child_runs_and_its_on_done_takes_its_output();
    a_child_s_error_reaches_on_error_and_unhandled_fails_the_parent();
    leaving_the_invoking_state_stops_the_child_and_its_own_children();
    a_config_action_named_like_a_built_in_is_a_custom_action();
    a_child_spawned_and_stopped_in_one_macrostep_never_starts();
    a_stopped_child_s_children_handle_what_it_sent_them_first();
    a_failed_child_s_children_keep_running_when_its_parent_stops_it();
    a_start_starts_the_initial_children_in_order_and_keeps_each_actors_actions_in_order();
    a_deferred_effect_that_fails_stops_the_children_its_macrostep_created();
    a_started_child_a_failing_macrostep_stopped_is_still_its_parent_s();
    return boost::report_errors();
}
