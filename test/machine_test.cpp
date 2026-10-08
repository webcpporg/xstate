// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Tests create_machine (doc: #xstate-invariant-1): the tree of state
 nodes it builds, with XState's ids, types and document order, the lookup of
 a node by id, and the configs and definitions it refuses.

 Tip: an explicit id names its own node only; its children's ids are the
 machine's id and their path, as in XState.
*/

#include <webcpp/xstate.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/json.hpp>

#include <array>
#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "require.hpp"

namespace xstate = webcpp::xstate;

using webcpp::test::noted;
using webcpp::test::require;

namespace {

boost::json::value parsed(std::string_view text) {
    boost::system::error_code failed;
    boost::json::value value = boost::json::parse(text, failed);
    require(noted(BOOST_TEST(!failed), "not JSON: ", text));
    return value;
}

std::string_view spelled(xstate::node_type type) {
    switch (type) {
        case xstate::node_type::atomic: return "atomic";
        case xstate::node_type::compound: return "compound";
        case xstate::node_type::parallel: return "parallel";
        case xstate::node_type::final: return "final";
        case xstate::node_type::history: return "history";
    }
    return "unknown";
}

constexpr std::string_view lookup_machine = R"({
    "initial": "start",
    "states": {
        "start": {},
        "stateWithDot": {"id": "foo.bar"},
        "foo": {
            "id": "foo",
            "initial": "bar",
            "states": {"bar": {}}
        },
        "q": {
            "id": "bar",
            "initial": "baz",
            "states": {
                "baz": {},
                "qux": {
                    "initial": "quux",
                    "states": {"quux": {}}
                }
            }
        }
    }
})";

struct lookup_row {
    std::string_view id;
    std::string_view node;
};

std::ostream& operator<<(std::ostream& out, const lookup_row& row) {
    return out << row.id;
}

// The node an id names, or the error a lookup that names none returns.
const std::array lookup_rows{
    lookup_row{
        .id = R"(#foo\.bar)",
        .node = "foo.bar",
    },
    lookup_row{
        .id = "#foo.bar",
        .node = "(machine).foo.bar",
    },
    lookup_row{
        .id = "#foo",
        .node = "foo",
    },
    lookup_row{
        .id = "#bar.qux.quux",
        .node = "(machine).q.qux.quux",
    },
    lookup_row{
        .id = "#(machine)",
        .node = "(machine)",
    },
    lookup_row{
        .id = "#nothing",
        .node = "unknown_state",
    },
    lookup_row{
        .id = "#foo.nothing",
        .node = "unknown_state",
    },
};

struct refusal_row {
    std::string_view config;
    std::string_view error;
};

std::ostream& operator<<(std::ostream& out, const refusal_row& row) {
    return out << row.config;
}

const std::array refusal_rows{
    refusal_row{
        .config =
            R"({
                "initial": "left",
                "states": {"left": {}, "right": {}},
                "on": {"CLICK": "left"}
            })",
        .error = "unknown_target",
    },
    refusal_row{
        .config = R"({"states": {"a": {}}})",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({
            "initial": "nothing",
            "states": {"a": {}}
        })",
        .error = "unknown_target",
    },
    refusal_row{
        .config = R"({
            "initial": "a",
            "states": {"a": {"on": {"": "a"}}}
        })",
        .error = "invalid_config",
    },
    refusal_row{
        .config =
            R"({
                "initial": "a",
                "states": {
                    "a": {
                        "on": {"E": {"target": "b", "guard": "absent"}}
                    },
                    "b": {}
                }
            })",
        .error = "unknown_guard",
    },
    refusal_row{
        .config = R"({
            "initial": "a",
            "states": {
                "a": {"after": {"never": "b"}},
                "b": {}
            }
        })",
        .error = "unknown_delay",
    },
    // An after key that starts with a digit, a dot or a sign is a number of
    // milliseconds, which it is only as digits alone; XState would read
    // "1.5" as a fraction and "5abc" as the name of a delay.
    refusal_row{
        .config = R"({
            "initial": "a",
            "states": {"a": {"after": {"1.5": "b"}}, "b": {}}
        })",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({
            "initial": "a",
            "states": {"a": {"after": {"-5": "b"}}, "b": {}}
        })",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({
            "initial": "a",
            "states": {"a": {"after": {"5abc": "b"}}, "b": {}}
        })",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({
            "initial": "a",
            "states": {"a": {"on": {"E": "#absent"}}}
        })",
        .error = "unknown_target",
    },
    refusal_row{
        .config = R"(["not", "an", "object"])",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({
            "initial": "h",
            "states": {"h": {"type": "history"}, "a": {}}
        })",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({
            "initial": "a",
            "states": {
                "a": {
                    "initial": "h1",
                    "states": {
                        "h1": {"type": "history", "target": "h2"},
                        "h2": {"type": "history", "target": "h1"}
                    }
                }
            }
        })",
        .error = "invalid_config",
    },
    refusal_row{
        .config =
            R"({
                "id": "m",
                "initial": "p",
                "states": {
                    "p": {
                        "id": "pp",
                        "initial": "h",
                        "states": {"h": {"type": "history", "target": "#pp"}, "x": {}}
                    }
                }
            })",
        .error = "invalid_config",
    },
    // An invoke names its actor by a string src, its id and systemId are strings.
    refusal_row{
        .config = R"({"invoke": {"src": {"type": "inline"}}})",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({"invoke": [{"id": "kid"}]})",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({"invoke": {"src": "child", "id": 7}})",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({"invoke": {"src": "child", "systemId": true}})",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({"invoke": ["child"]})",
        .error = "invalid_config",
    },
    refusal_row{
        .config =
            R"({
                "initial": "a",
                "states": {
                    "a": {"invoke": {"src": "child", "onDone": "#nowhere"}}
                }
            })",
        .error = "unknown_target",
    },
    // An invoke's src names an actor of the implementations.
    refusal_row{
        .config = R"({"invoke": {"src": "absent"}})",
        .error = "unknown_actor",
    },
    // A history's target is a string or a list of strings, the root's too.
    refusal_row{
        .config = R"({"type": "history", "target": 5})",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({"type": "history", "target": [5]})",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({"type": "history", "target": true})",
        .error = "invalid_config",
    },
    // XState reads a null entry, exit, actions or tags as a list that holds
    // null (toArray), and throws where it runs the null action.
    refusal_row{
        .config = R"({"entry": null})",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({"initial": "a", "states": {"a": {"exit": null}}})",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({"tags": null})",
        .error = "invalid_config",
    },
    // A tags is a string or a list of strings, and holds nothing else.
    refusal_row{
        .config = R"({"tags": ["busy", 1]})",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({"tags": 5})",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({
            "initial": "a",
            "states": {"a": {"on": {"GO": {"target": "a", "actions": null}}}}
        })",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({"initial": {"target": "a", "actions": null}, "states": {"a": {}}})",
        .error = "invalid_config",
    },
    // So does XState's normalizeTarget read a history's null target, and it
    // throws where it needs the default.
    refusal_row{
        .config = R"({
            "initial": "a",
            "states": {
                "a": {
                    "initial": "x",
                    "states": {"x": {}, "h": {"type": "history", "target": null}}
                }
            }
        })",
        .error = "invalid_config",
    },
    refusal_row{
        .config = R"({"type": "history", "target": null})",
        .error = "invalid_config",
    },
};

const std::array<std::string_view, 7> malformed_definitions{
    {
        R"({
            "id": "m",
            "initial": {"target": [1]},
            "states": {"a": {"id": "m.a"}}
        })",
        R"({
            "id": "m",
            "on": {"E": [1]},
            "states": {}
        })",
        R"({"id": "m", "on": {"E": 1}, "states": {"a": 2}})",
        R"(["not", "a", "definition"])",
        R"({"id": "m", "entry": "notify", "states": {}})",
        R"({"id": "m", "exit": 5, "states": {}})",
        R"({
            "id": "m",
            "initial": {"target": ["#m.a"]},
            "states": {"a": {"id": "m.a", "entry": {"type": "notify"}}}
        })",
    },
};

void every_node_has_xstates_id_type_and_order() {
    const boost::json::value config = parsed(R"({
        "id": "m",
        "initial": "a",
        "states": {
            "a": {
                "initial": "a1",
                "states": {"a1": {}, "a2": {"type": "final"}}
            },
            "b": {
                "id": "explicit",
                "type": "parallel",
                "states": {"r1": {}, "r2": {}}
            },
            "h": {"history": "deep"}
        }
    })");
    const xstate::result<xstate::machine> machine =
        xstate::create_machine(config, xstate::implementations());
    if (!BOOST_TEST(machine.has_value())) {
        return;
    }
    constexpr std::array<std::pair<std::string_view, std::string_view>, 8> expected{
        {
            {"m", "compound"},
            {"m.a", "compound"},
            {"m.a.a1", "atomic"},
            {"m.a.a2", "final"},
            {"explicit", "parallel"},
            {"m.b.r1", "atomic"},
            {"m.b.r2", "atomic"},
            {"m.h", "history"},
        },
    };
    if (!BOOST_TEST_EQ(machine->size(), expected.size())) {
        return;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        noted(BOOST_TEST_EQ(machine->node(index).id, expected.at(index).first), "node ", index);
        noted(BOOST_TEST_EQ(spelled(machine->node(index).type), expected.at(index).second), "node ",
              index);
    }
}

// Each data case of the old suite is a function of one row, called for every row of the same
// data, in order: a failed check names its row on the line after it, and a failed required
// check ends its row, as Boost.Test ended the row's case.

void a_state_id_finds_its_node_escapes_included(const lookup_row& row) {
    const xstate::result<xstate::machine> machine =
        xstate::create_machine(parsed(lookup_machine), xstate::implementations());
    if (!noted(BOOST_TEST(machine.has_value()), "for ", row)) {
        return;
    }
    const xstate::result<std::size_t> found = machine->node_by_id(row.id);
    const std::string actual =
        found.has_value() ? machine->node(*found).id : found.error().message();
    noted(BOOST_TEST_EQ(actual, row.node), "for ", row);
}

void a_state_id_finds_its_node_escapes_included() {
    for (const lookup_row& row : lookup_rows) {
        a_state_id_finds_its_node_escapes_included(row);
    }
}

/** Implementations that hold one actor, `child`, which the refusals may name. */
xstate::implementations with_child() {
    xstate::implementations registry;
    registry.actors.emplace("child", xstate::host_actor{});
    return registry;
}

void a_config_that_is_not_a_machine_is_refused(const refusal_row& row) {
    const xstate::result<xstate::machine> machine =
        xstate::create_machine(parsed(row.config), with_child());
    if (!noted(BOOST_TEST(!machine.has_value()), "for ", row)) {
        return;
    }
    noted(BOOST_TEST_EQ(machine.error().message(), row.error), "for ", row);
}

void a_config_that_is_not_a_machine_is_refused() {
    for (const refusal_row& row : refusal_rows) {
        a_config_that_is_not_a_machine_is_refused(row);
    }
}

void a_definition_that_is_not_one_is_refused_never_aborts(std::string_view definition) {
    const xstate::result<xstate::machine> machine = xstate::create_machine_from_definition(
        parsed(definition), xstate::implementations(), boost::json::value(boost::json::object()));
    if (!noted(BOOST_TEST(!machine.has_value()), "for ", definition)) {
        return;
    }
    noted(BOOST_TEST_EQ(machine.error().message(), "invalid_config"), "for ", definition);
}

void a_definition_that_is_not_one_is_refused_never_aborts() {
    for (const std::string_view definition : malformed_definitions) {
        a_definition_that_is_not_one_is_refused_never_aborts(definition);
    }
}

// A definition gives back the version of the machine it was written from,
// so the machine loaded from it writes the same definition.
void a_definition_keeps_the_machines_version() {
    const boost::json::value config = parsed(R"({
        "id": "m",
        "version": "1.2",
        "initial": "a",
        "states": {"a": {"on": {"GO": "b"}}, "b": {}}
    })");
    const xstate::result<xstate::machine> machine =
        xstate::create_machine(config, xstate::implementations());
    if (!BOOST_TEST(machine.has_value())) {
        return;
    }
    const boost::json::value written = xstate::to_json(*machine);
    const xstate::result<xstate::machine> reloaded = xstate::create_machine_from_definition(
        written, xstate::implementations(), boost::json::value(boost::json::object()));
    if (!BOOST_TEST(reloaded.has_value())) {
        return;
    }
    BOOST_TEST_EQ(xstate::to_json(*reloaded), written);
}

// An initial transition's meta and description, which a definition writes,
// come back with the machine loaded from it.
void a_definition_keeps_an_initial_transitions_meta_and_description() {
    const boost::json::value config = parsed(R"({
        "id": "m",
        "initial": {"target": "a", "meta": {"step": 1}, "description": "where it starts"},
        "states": {"a": {}}
    })");
    const xstate::result<xstate::machine> machine =
        xstate::create_machine(config, xstate::implementations());
    if (!BOOST_TEST(machine.has_value())) {
        return;
    }
    const boost::json::value written = xstate::to_json(*machine);
    const xstate::result<xstate::machine> reloaded = xstate::create_machine_from_definition(
        written, xstate::implementations(), boost::json::value(boost::json::object()));
    if (!BOOST_TEST(reloaded.has_value())) {
        return;
    }
    BOOST_TEST_EQ(xstate::to_json(*reloaded), written);
}

// A state value is a JavaScript object in XState, whose keys that are array
// indices come first, by number, whatever order the regions were written in.
void a_state_value_lists_its_keys_as_javascript_enumerates_them() {
    const boost::json::value config = parsed(R"({
        "id": "m",
        "type": "parallel",
        "states": {
            "z": {"initial": "z1", "states": {"z1": {}, "z2": {}}},
            "3": {"initial": "c", "states": {"c": {}, "d": {}}}
        }
    })");
    const xstate::result<xstate::machine> machine =
        xstate::create_machine(config, xstate::implementations());
    if (!BOOST_TEST(machine.has_value())) {
        return;
    }
    BOOST_TEST_EQ(boost::json::serialize(xstate::get_initial_snapshot(*machine).value),
                  R"({"3":"c","z":"z1"})");
    const xstate::result<xstate::snapshot> resolved =
        xstate::resolve_state(*machine, parsed(R"({"z": "z2", "3": "d"})"), boost::json::object());
    if (!BOOST_TEST(resolved.has_value())) {
        return;
    }
    BOOST_TEST_EQ(boost::json::serialize(resolved->value), R"({"3":"d","z":"z2"})");
}

// A transition declaring `cond`, renamed `guard` in v5, is refused when
// JavaScript reads it as true, as XState's development build refuses it
// (stateUtils.ts formatTransition), and ignored otherwise.
void a_truthy_cond_is_refused_and_a_falsy_one_ignored() {
    const auto with_cond = [](std::string_view cond) {
        return xstate::create_machine(
            parsed(R"({"initial": "a", "states": {"a": {"on": {"GO": {"target": "b", "cond": )" +
                   std::string(cond) + R"(}}}, "b": {}}})"),
            xstate::implementations());
    };
    for (const std::string_view truthy : {R"("never")", "true", "1", "{}", "[]"}) {
        const xstate::result<xstate::machine> refused = with_cond(truthy);
        if (!noted(BOOST_TEST(!refused.has_value()), "for ", truthy)) {
            return;
        }
        noted(BOOST_TEST_EQ(refused.error().message(), "invalid_config"), "for ", truthy);
    }
    for (const std::string_view falsy : {"false", "0", R"("")", "null"}) {
        noted(BOOST_TEST(with_cond(falsy).has_value()), "for ", falsy);
    }
}

void a_spawn_child_action_names_an_actor_of_the_implementations() {
    xstate::implementations registry;
    registry.actions.emplace("spawn", xstate::spawn_child_action{
                                          .src = "absent",
                                          .id = "kid",
                                          .system_id = std::nullopt,
                                          .input = std::nullopt,
                                      });
    const xstate::result<xstate::machine> machine =
        xstate::create_machine(parsed(R"({"entry": ["spawn"]})"), registry);
    if (!BOOST_TEST(!machine.has_value())) {
        return;
    }
    BOOST_TEST_EQ(machine.error().message(), "unknown_actor");
}

// An implementation holding an empty function is refused with the machine,
// as calling it would abort without exceptions; an optional one left out is
// not (doc: #xstate-invariant-1).
void an_implementation_holding_an_empty_function_is_refused() {
    std::vector<std::pair<std::string_view, xstate::implementations>> registries;
    const auto with_action = [&registries](std::string_view name,
                                           xstate::action_implementation broken) {
        xstate::implementations registry;
        registry.actions.emplace("broken", std::move(broken));
        registries.emplace_back(name, std::move(registry));
    };
    with_action("assign", xstate::assign_action{.assignment = {}});
    with_action("raise",
                xstate::raise_action{.event = {}, .id = std::nullopt, .delay = std::nullopt});
    with_action("log", xstate::log_action{.value = xstate::value_maker(), .label = std::nullopt});
    with_action("sendParent",
                xstate::send_parent_action{.event = {}, .id = std::nullopt, .delay = std::nullopt});
    with_action("spawnChild", xstate::spawn_child_action{
                                  .src = "kid",
                                  .id = "kid",
                                  .system_id = std::nullopt,
                                  .input = xstate::value_maker(),
                              });
    with_action("sendTo", xstate::send_to_action{
                              .target = "kid",
                              .event = {},
                              .id = std::nullopt,
                              .delay = std::nullopt,
                          });
    with_action("emit", xstate::emit_action{.event = {}});
    xstate::implementations guard;
    guard.guards.emplace("broken", xstate::predicate());
    registries.emplace_back("guard", std::move(guard));
    xstate::implementations delay;
    delay.delays.emplace("broken", xstate::delay_maker());
    registries.emplace_back("delay", std::move(delay));
    xstate::implementations context;
    context.context = xstate::context_maker();
    registries.emplace_back("context", std::move(context));
    with_action("spawnChild id", xstate::spawn_child_action{
                                     .src = "kid",
                                     .id = xstate::id_maker(),
                                     .system_id = std::nullopt,
                                     .input = std::nullopt,
                                 });
    xstate::implementations input;
    input.inputs.emplace("broken", xstate::value_maker());
    registries.emplace_back("input", std::move(input));
    xstate::implementations output;
    output.outputs.emplace("broken", xstate::value_maker());
    registries.emplace_back("output", std::move(output));
    for (const auto& [name, registry] : registries) {
        const xstate::result<xstate::machine> made =
            xstate::create_machine(parsed(R"({"id": "m"})"), registry);
        noted(BOOST_TEST(!made.has_value()), "implementation ", name);
        noted(BOOST_TEST(!made.has_value() && made.error().message() == "invalid_config"),
              "implementation ", name);
    }
    xstate::implementations left_out;
    left_out.actions.emplace("log", xstate::log_action{.value = std::nullopt, .label = "here"});
    BOOST_TEST(xstate::create_machine(parsed(R"({"id": "m"})"), left_out).has_value());
}

// A computed input names an invoke and a computed output a final state or
// the root, neither of which may also have a fixed one in the config
// (doc: #xstate-invariant-24).
void a_computed_input_or_output_names_what_it_computes() {
    const auto made = [](std::string_view config, std::string_view field, std::string_view id) {
        xstate::implementations registry;
        registry.actors.emplace("child", xstate::host_actor{});
        auto& computed = field == "inputs" ? registry.inputs : registry.outputs;
        computed.emplace(std::string(id), [](const xstate::action_args&) {
            return xstate::result<boost::json::value>(1);
        });
        return xstate::create_machine(parsed(config), std::move(registry));
    };
    constexpr std::string_view invoking = R"({
        "id": "m",
        "invoke": {"src": "child", "id": "kid"}
    })";
    constexpr std::string_view finishing =
        R"({
            "id": "m",
            "initial": "a",
            "states": {"a": {}, "end": {"type": "final"}}
        })";
    BOOST_TEST(made(invoking, "inputs", "kid").has_value());
    BOOST_TEST(made(finishing, "outputs", "m.end").has_value());
    BOOST_TEST(made(finishing, "outputs", "m").has_value());
    for (const auto& [config, field, id] :
         std::vector<std::tuple<std::string_view, std::string_view, std::string_view>>{
             {invoking, "inputs", "absent"},
             {
                 R"({"id": "m", "invoke": {"src": "child", "id": "kid", "input": 2}})",
                 "inputs",
                 "kid",
             },
             {finishing, "outputs", "absent"},
             {finishing, "outputs", "m.a"},
             {
                 R"({
                     "id": "m",
                     "initial": "a",
                     "states": {"a": {}, "end": {"type": "final", "output": 2}}
                 })",
                 "outputs",
                 "m.end",
             },
         }) {
        const xstate::result<xstate::machine> refused = made(config, field, id);
        noted(BOOST_TEST(!refused.has_value()), field, " ", id, " for ", config);
        noted(BOOST_TEST(!refused.has_value() && refused.error().message() == "invalid_config"),
              field, " ", id, " for ", config);
    }
}

// A registered spawn_child_action the config never names is not checked, as
// XState's setup accepts it; one a transition or an always names is.
void only_a_spawn_child_action_the_config_names_must_name_a_known_actor() {
    xstate::implementations registry;
    registry.actions.emplace("spawn", xstate::spawn_child_action{
                                          .src = "absent",
                                          .id = "kid",
                                          .system_id = std::nullopt,
                                          .input = std::nullopt,
                                      });
    BOOST_TEST(xstate::create_machine(parsed(R"({"id": "unused"})"), registry).has_value());
    for (const std::string_view config : {
             R"({"on": {"GO": {"actions": ["spawn"]}}})",
             R"({
                 "initial": "a",
                 "states": {
                     "a": {"always": {"target": "b", "actions": "spawn"}},
                     "b": {}
                 }
             })",
         }) {
        const xstate::result<xstate::machine> named =
            xstate::create_machine(parsed(config), registry);
        if (!BOOST_TEST(!named.has_value())) {
            return;
        }
        BOOST_TEST_EQ(named.error().message(), "unknown_actor");
    }
}

void a_machine_actor_holds_the_machine_it_runs() {
    const xstate::result<xstate::machine> child =
        xstate::create_machine(parsed(R"({"id": "child"})"), xstate::implementations());
    if (!BOOST_TEST(child.has_value())) {
        return;
    }
    const xstate::machine_actor actor{*child};
    BOOST_TEST_EQ(actor.logic->id(), "child");
}

}  // namespace

int main() {
    every_node_has_xstates_id_type_and_order();
    a_state_id_finds_its_node_escapes_included();
    a_config_that_is_not_a_machine_is_refused();
    a_definition_that_is_not_one_is_refused_never_aborts();
    a_definition_keeps_the_machines_version();
    a_definition_keeps_an_initial_transitions_meta_and_description();
    a_state_value_lists_its_keys_as_javascript_enumerates_them();
    a_truthy_cond_is_refused_and_a_falsy_one_ignored();
    a_spawn_child_action_names_an_actor_of_the_implementations();
    an_implementation_holding_an_empty_function_is_refused();
    a_computed_input_or_output_names_what_it_computes();
    only_a_spawn_child_action_the_config_names_must_name_a_known_actor();
    a_machine_actor_holds_the_machine_it_runs();
    return boost::report_errors();
}
