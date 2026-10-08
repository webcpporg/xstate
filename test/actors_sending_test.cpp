// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Tests the events xstate's actors send each other and the system that
 delivers them (doc: #xstate-on-xactor): a sendTo, a sendParent and a send to
 the actor itself, an error sent as an event, and the systemIds that name
 actors while they run.

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
using webcpp::test::xstate_actors::sending;
using webcpp::test::xstate_actors::sending_up;
using webcpp::test::xstate_actors::settles;
using webcpp::test::xstate_actors::started;
using webcpp::test::xstate_actors::with_actors;
using webcpp::test::xstate_actors::with_host;

namespace {

void a_parent_sends_to_its_child_and_the_child_answers_its_parent() {
    xstate::implementations child_registry = vocabulary::of_case({});
    child_registry.actions.emplace("pong", sending_up("PONG"));
    const xstate::machine echo =
        machine_of(R"({"on": {"PING": {"actions": ["pong"]}}})", std::move(child_registry));
    xstate::implementations registry = with_actors({{"echo", echo}});
    registry.actions.emplace("ping", sending("kid", "PING"));
    const xstate::machine parent = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {
                "invoke": {"src": "echo", "id": "kid"},
                "on": {
                    "GO": {"actions": ["ping"]},
                    "PONG": "b"
                }
            },
            "b": {}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    if (!BOOST_TEST(system.send(root, named("GO")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("b")"));
}

void a_system_id_names_its_actor_while_it_runs() {
    xstate::implementations registry = with_actors({{"worker", machine_of(R"({})")}});
    registry.actions.emplace("tell", sending("#system:helper", "NOTE"));
    registry.actions.emplace("tell_nobody", sending("#system:nobody", "TO_SELF"));
    const xstate::machine parent = machine_of(R"({
        "context": {},
        "initial": "a",
        "states": {
            "a": {
                "invoke": {"src": "worker", "id": "kid", "systemId": "helper"},
                "on": {
                    "LEAVE": "b",
                    "NOBODY": {"actions": ["tell_nobody"]},
                    "TO_SELF": {
                        "actions": [
                            {"type": "set", "params": {"key": "self", "value": true}}
                        ]
                    }
                }
            },
            "b": {}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root =
        started(system, parent, {.input = nullptr, .id = std::nullopt, .system_id = "app"});
    BOOST_TEST(system.get("app") == std::optional(root));
    BOOST_TEST(system.get("helper") == system.child_of(root, "kid"));

    // A systemId that names no actor sends to the actor itself, as XState's
    // system.get() returning undefined does.
    if (!BOOST_TEST(system.send(root, named("NOBODY")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->context, parsed(R"({"self": true})"));

    if (!BOOST_TEST(system.send(root, named("LEAVE")).has_value())) {
        return;
    }
    BOOST_TEST(!system.get("helper").has_value());
}

void a_system_id_claimed_twice_fails_the_actor_that_spawns_the_second() {
    const xstate::machine parent = machine_of(R"({
        "invoke": [
            {"src": "worker", "id": "one", "systemId": "dup"},
            {"src": "worker", "id": "two", "systemId": "dup"}
        ]
    })",
                                              with_actors({{"worker", machine_of(R"({})")}}));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    BOOST_TEST(system.status_of(root).value() == xactor::status::error);
    BOOST_TEST_EQ(system.snapshot_of(root)->error.message(), "system_id_taken");
    // XState throws while resolving the second spawn, so no effect of the
    // macrostep runs: neither child starts.
    BOOST_TEST(!system.child_of(root, "one").has_value());
    BOOST_TEST(system.snapshot_of(root)->children.empty());
}

// An entry sendTo to its own state's invoke is resolved before the child
// exists; the child gets it after its start, as in XState.
void an_entry_send_to_its_own_invoke_reaches_the_child_after_its_start() {
    const xstate::machine listener = machine_of(
        R"({
            "entry": ["child_started"],
            "on": {
                "HELLO": {"actions": ["child_got_hello"]}
            }
        })");
    xstate::implementations registry = with_actors({{"listener", listener}});
    registry.actions.emplace("hello", sending("kid", "HELLO"));
    const xstate::machine parent = machine_of(R"({
        "initial": "idle",
        "states": {
            "idle": {"on": {"GO": "b"}},
            "b": {"entry": ["hello"], "invoke": {"src": "listener", "id": "kid"}}
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
    const std::vector<std::string> expected{"child_started", "child_got_hello"};
    BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), expected.begin(), expected.end());
}

// XState rewrites a sent xstate.error into the sender's error event.
void a_child_s_xstate_error_reaches_its_parent_as_its_error() {
    xstate::implementations child_registry = vocabulary::of_case({});
    child_registry.actions.emplace("complain", sending_up("xstate.error", {{"data", "payload"}}));
    const xstate::machine complainer = machine_of(R"({
        "on": {"COMPLAIN": {"actions": ["complain"]}}
    })",
                                                  std::move(child_registry));
    const xstate::machine parent = machine_of(R"({
        "context": {},
        "initial": "a",
        "states": {
            "a": {
                "invoke": {
                    "src": "complainer",
                    "id": "kid",
                    "onError": {
                        "target": "b",
                        "actions": [
                            {
                                "type": "set_from_event",
                                "params": {"key": "reason", "from": "error"}
                            }
                        ]
                    }
                }
            },
            "b": {}
        }
    })",
                                              with_actors({{"complainer", complainer}}));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    if (!BOOST_TEST(system.send(child(system, root, "kid"), named("COMPLAIN")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->context, parsed(R"({"reason": "payload"})"));
}

void a_send_to_internal_reaches_the_actor_as_an_event_of_its_own() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("again", sending("#_internal", "AGAIN"));
    const xstate::machine looping = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {
                "on": {
                    "GO": {"target": "b", "actions": ["again"]}
                }
            },
            "b": {"on": {"AGAIN": "c"}},
            "c": {}
        }
    })",
                                               std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, looping);
    if (!BOOST_TEST(system.send(root, named("GO")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("c")"));
}

void a_send_parent_from_a_root_fails_it() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("up", sending_up("UP"));
    const xstate::machine orphan = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {
                "on": {
                    "GO": {"target": "b", "actions": ["up"]}
                }
            },
            "b": {}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, orphan);
    if (!BOOST_TEST(system.send(root, named("GO")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(root).value() == xactor::status::error);
    BOOST_TEST_EQ(system.snapshot_of(root)->error.message(), "unknown_target");
    // XState's resolution throws, so the actor keeps the snapshot it had.
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("a")"));
}

void a_send_to_a_missing_child_fails_the_parent() {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actions.emplace("nope", sending("nobody", "HELLO"));
    const xstate::machine parent =
        machine_of(R"({"on": {"GO": {"actions": ["nope"]}}})", std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    if (!BOOST_TEST(system.send(root, named("GO")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(root).value() == xactor::status::error);
}

// XState relays an exit action's sendTo before it stops the state's child,
// which handles the event first (predictableExec.test.ts, "should deliver
// events sent from the exit actions to a service invoked in the same state").
void an_exit_send_to_its_own_child_reaches_it_before_the_stop() {
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
            "b": {}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    const xstate::actor_ref kid = child(system, root, "kid");
    if (!BOOST_TEST(system.send(root, named("LEAVE")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(kid).value() == xactor::status::stopped);
    BOOST_TEST_EQ(system.snapshot_of(kid)->context, parsed(R"({"log": ["bye"]})"));
    BOOST_TEST(!system.child_of(root, "kid").has_value());
}

// XState delivers what a parent sends at once, so what an earlier
// macrostep sent a child reaches it before a later macrostep stops it.
void a_stop_comes_after_what_an_earlier_macrostep_sent_the_child() {
    xstate::implementations kicker_registry = vocabulary::of_case({});
    kicker_registry.actions.emplace("ping", sending_up("PING"));
    kicker_registry.actions.emplace("stop", sending_up("STOP"));
    const xstate::machine kicker = machine_of(R"({
        "on": {"GO": {"actions": ["ping", "stop"]}}
    })",
                                              std::move(kicker_registry));
    const xstate::machine listener = machine_of(R"({
        "context": {"log": []},
        "on": {
            "PING": {
                "actions": [
                    {"type": "push", "params": {"key": "log", "value": "ping"}}
                ]
            }
        }
    })");
    xstate::implementations registry = with_actors({{"kicker", kicker}, {"listener", listener}});
    registry.actions.emplace("forward", sending("c", "PING"));
    const xstate::machine parent = machine_of(R"({
        "invoke": {"src": "kicker", "id": "kicker"},
        "initial": "running",
        "states": {
            "running": {
                "invoke": {"src": "listener", "id": "c"},
                "on": {
                    "PING": {"actions": ["forward"]},
                    "STOP": "idle"
                }
            },
            "idle": {}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    const xstate::actor_ref c = child(system, root, "c");
    if (!BOOST_TEST(system.send(child(system, root, "kicker"), named("GO")).has_value())) {
        return;
    }
    BOOST_TEST_EQ(system.snapshot_of(root)->value, parsed(R"("idle")"));
    BOOST_TEST(system.status_of(c).value() == xactor::status::stopped);
    BOOST_TEST_EQ(system.snapshot_of(c)->context, parsed(R"({"log": ["ping"]})"));
}

// Stopping an ended child releases only the names its family still holds.
void stopping_an_ended_child_leaves_a_system_id_another_actor_took() {
    xstate::implementations registry = with_host();
    registry.actions.emplace("spawn_x", xstate::spawn_child_action{
                                            .src = "fetchUser",
                                            .id = "x",
                                            .system_id = "svc",
                                            .input = std::nullopt,
                                        });
    registry.actions.emplace("bye", sending("c", "BYE"));
    const xstate::machine parent = machine_of(R"({
        "initial": "a",
        "states": {
            "a": {
                "invoke": {"src": "fetchUser", "id": "c", "systemId": "svc"},
                "exit": ["bye"],
                "on": {
                    "SPAWN": {"actions": ["spawn_x"]},
                    "LEAVE": "b"
                }
            },
            "b": {}
        }
    })",
                                              std::move(registry));
    xstate::actor_system system(plenty);
    const xstate::actor_ref root = started(system, parent);
    if (!BOOST_TEST(system.resolve(system.host_requests().front(), std::nullopt).has_value())) {
        return;
    }
    if (!BOOST_TEST(system.send(root, named("SPAWN")).has_value())) {
        return;
    }
    const xstate::actor_ref x = child(system, root, "x");
    if (!BOOST_TEST(system.send(root, named("LEAVE")).has_value())) {
        return;
    }
    BOOST_TEST(system.get("svc") == std::optional(x));
}

// A systemId a child spawned in the same macrostep held is free again once
// that macrostep stops it, as XState unregisters it at once; not in the
// initial macrostep, where XState defers the stop and refuses the claim.
void a_system_id_freed_in_the_same_macrostep_can_be_claimed_again() {
    constexpr std::string_view config = R"({
        "initial": "%",
        "states": {
            "idle": {"on": {"GO": "a"}},
            "a": {
                "invoke": {"id": "first", "src": "fetchUser", "systemId": "svc"},
                "always": "b"
            },
            "b": {"invoke": {"id": "second", "src": "fetchUser", "systemId": "svc"}}
        }
    })";
    const auto with_initial = [config](std::string_view initial) {
        std::string text(config);
        text.replace(text.find('%'), 1, initial);
        return machine_of(text, with_host());
    };
    xstate::actor_system system(plenty);
    const xstate::actor_ref later = started(system, with_initial("idle"));
    if (!BOOST_TEST(system.send(later, named("GO")).has_value())) {
        return;
    }
    BOOST_TEST(system.status_of(later).value() == xactor::status::active);
    BOOST_TEST(system.get("svc") == system.child_of(later, "second"));

    xstate::actor_system other(plenty);
    const xstate::actor_ref initial = started(other, with_initial("a"));
    BOOST_TEST(other.status_of(initial).value() == xactor::status::error);
    BOOST_TEST_EQ(other.snapshot_of(initial)->error.message(), "system_id_taken");
}

// A child born done or failed released its systemId when it was
// constructed, and its start finds it held by a later child of the same
// macrostep, as XState's start() does when it claims it again
// (createActor.ts, system._set): the parent fails with system_id_taken. The
// root's start has started its active children first, as XState's
// StateMachine.start does, so the later child runs on and keeps the name.
void a_child_born_ended_claims_its_system_id_again_when_started() {
    for (const std::string_view born : {
             R"({
                 "initial": "f",
                 "states": {"f": {"type": "final"}}
             })",
             R"({"context": {}, "entry": ["fail"]})",
         }) {
        xstate::implementations registry =
            with_actors({{"ended", machine_of(born)}, {"running", machine_of(R"({})")}});
        registry.actions.emplace("spawn_ended", xstate::spawn_child_action{
                                                    .src = "ended",
                                                    .id = "a",
                                                    .system_id = "x",
                                                    .input = std::nullopt,
                                                });
        registry.actions.emplace("spawn_running", xstate::spawn_child_action{
                                                      .src = "running",
                                                      .id = "b",
                                                      .system_id = "x",
                                                      .input = std::nullopt,
                                                  });
        const xstate::machine root_machine = machine_of(
            R"({"context": {}, "entry": ["spawn_ended", "spawn_running"]})", std::move(registry));
        xstate::actor_system system(plenty);
        const xstate::actor_ref root = started(system, root_machine, xstate::actor_options(), born);
        noted(BOOST_TEST(system.status_of(root).value() == xactor::status::error), born);
        noted(BOOST_TEST_EQ(system.snapshot_of(root)->error.message(), "system_id_taken"), born);
        const xstate::actor_ref later = child(system, root, "b", born);
        noted(BOOST_TEST(system.status_of(later).value() == xactor::status::active), born);
        noted(BOOST_TEST(system.get("x") == std::optional(later)), born);
    }
}

// A child that ends while its report to its parent waits for fuel has
// already released its systemId, as XState unregisters an actor the moment
// it is done (doc: #xstate-invariant-a6).
void a_child_done_while_its_report_waits_for_fuel_has_released_its_system_id() {
    const xstate::machine worker = machine_of(R"({
        "initial": "working",
        "states": {
            "working": {"on": {"FINISH": "finished"}},
            "finished": {"type": "final"}
        }
    })");
    const xstate::machine parent = machine_of(R"({
        "invoke": {
            "src": "worker",
            "id": "kid",
            "systemId": "worker",
            "onDone": {"actions": ["kid_done"]}
        }
    })",
                                              with_actors({{"worker", worker}}));
    xstate::actor_system system(xactor::budgets{.fuel = 1});
    action_record record;
    record_actions(system, record);
    const xstate::actor_ref root = system.create_actor(parent).value();
    if (!BOOST_TEST(settles(system, system.start(root)))) {
        return;
    }
    const xstate::actor_ref kid = child(system, root, "kid");
    BOOST_TEST(system.get("worker") == std::optional(kid));

    // FINISH pays the kid's microstep; its done report waits for fuel.
    BOOST_TEST(system.send(kid, named("FINISH")).value() == xstate::run_outcome::out_of_fuel);
    BOOST_TEST(system.snapshot_of(kid)->status == xstate::status::done);
    BOOST_TEST(!system.get("worker").has_value());

    if (!BOOST_TEST(settles(system, system.resume()))) {
        return;
    }
    BOOST_TEST(system.status_of(kid).value() == xactor::status::done);
    const std::vector<std::string> done{"kid_done"};
    BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), done.begin(), done.end());
}

// The host cannot create an actor under a systemId a running actor holds;
// the name is free again once that actor has ended (doc: #xstate-invariant-a8).
void create_actor_refuses_a_system_id_a_running_actor_holds() {
    const xstate::machine idle = machine_of(R"({})");
    const xstate::actor_options named_app{.input = nullptr, .id = std::nullopt, .system_id = "app"};
    xstate::actor_system system(plenty);
    const xstate::actor_ref first = started(system, idle, named_app);
    const xstate::result<xstate::actor_ref> refused = system.create_actor(idle, named_app);
    BOOST_TEST(!refused.has_value());
    BOOST_TEST(!refused.has_value() && refused.error().message() == "system_id_taken");
    BOOST_TEST(system.get("app") == std::optional(first));
    if (!BOOST_TEST(system.stop(first).has_value())) {
        return;
    }
    BOOST_TEST(system.create_actor(idle, named_app).has_value());
}

// A child that fails while its error report waits for fuel has released its
// systemId too, as XState's _error unregisters it at once
// (doc: #xstate-invariant-a6).
void a_child_failed_while_its_report_waits_for_fuel_has_released_its_system_id() {
    const xstate::machine worker = machine_of(R"({
        "context": {},
        "on": {"BREAK": {"actions": ["fail"]}}
    })");
    const xstate::machine parent = machine_of(R"({
        "invoke": {
            "src": "worker",
            "id": "kid",
            "systemId": "worker",
            "onError": {"actions": ["kid_failed"]}
        }
    })",
                                              with_actors({{"worker", worker}}));
    xstate::actor_system system(xactor::budgets{.fuel = 1});
    action_record record;
    record_actions(system, record);
    const xstate::actor_ref root = system.create_actor(parent).value();
    if (!BOOST_TEST(settles(system, system.start(root)))) {
        return;
    }
    const xstate::actor_ref kid = child(system, root, "kid");

    // BREAK pays the kid's microstep, which fails; its error report waits.
    BOOST_TEST(system.send(kid, named("BREAK")).value() == xstate::run_outcome::out_of_fuel);
    BOOST_TEST(system.snapshot_of(kid)->status == xstate::status::error);
    BOOST_TEST(!system.get("worker").has_value());

    if (!BOOST_TEST(settles(system, system.resume()))) {
        return;
    }
    BOOST_TEST(system.status_of(kid).value() == xactor::status::error);
    const std::vector<std::string> failed{"kid_failed"};
    BOOST_TEST_ALL_EQ(record.types.begin(), record.types.end(), failed.begin(), failed.end());
}

}  // namespace

int main() {
    a_parent_sends_to_its_child_and_the_child_answers_its_parent();
    a_system_id_names_its_actor_while_it_runs();
    a_system_id_claimed_twice_fails_the_actor_that_spawns_the_second();
    an_entry_send_to_its_own_invoke_reaches_the_child_after_its_start();
    a_child_s_xstate_error_reaches_its_parent_as_its_error();
    a_send_to_internal_reaches_the_actor_as_an_event_of_its_own();
    a_send_parent_from_a_root_fails_it();
    a_send_to_a_missing_child_fails_the_parent();
    an_exit_send_to_its_own_child_reaches_it_before_the_stop();
    a_stop_comes_after_what_an_earlier_macrostep_sent_the_child();
    stopping_an_ended_child_leaves_a_system_id_another_actor_took();
    a_system_id_freed_in_the_same_macrostep_can_be_claimed_again();
    a_child_born_ended_claims_its_system_id_again_when_started();
    a_child_done_while_its_report_waits_for_fuel_has_released_its_system_id();
    create_actor_refuses_a_system_id_a_running_actor_holds();
    a_child_failed_while_its_report_waits_for_fuel_has_released_its_system_id();
    return boost::report_errors();
}
