// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Runs every case ported from XState's tests and compares every step with
 what XState produced for it: each microstep's event, transitions,
 actions and snapshot, the machine's definition, and, where the definition
 holds the whole machine, the machine loaded back from it.

 Tip: a case's machine runs one microstep at a time through the cursor, the
 way a caller paying fuel runs it, so the comparison proves the cursor and
 the functions built on it at once. The cases and XState's output are under
 WEBCPP_TEST_XSTATE_FIXTURES, which test/Jamfile defines.
*/

#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include "fixtures.hpp"
#include "vocabulary.hpp"

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ostream>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace xstate = webcpp::xstate;
namespace fixtures = webcpp::test::xstate_fixtures;
namespace vocabulary = webcpp::test::xstate_vocabulary;
namespace data = boost::unit_test::data;

/** A transition as the oracle serialises it. */
boost::json::object transition_json(const xstate::machine& machine,
                                    const xstate::transition_definition& taken) {
    boost::json::object json{
        {"source", "#" + machine.node(taken.source).id},
        {"eventType", taken.event_type},
    };
    if (taken.target.has_value()) {
        boost::json::array targets;
        for (const std::size_t node : *taken.target) {
            targets.emplace_back("#" + machine.node(node).id);
        }
        json.emplace("target", std::move(targets));
    }
    return json;
}

/** Actions as the oracle serialises them. */
boost::json::array actions_json(std::span<const xstate::action> actions) {
    boost::json::array listed;
    for (const xstate::action& one : actions) {
        boost::json::object action{{"type", one.type}};
        if (!one.params.is_null()) {
            action.emplace("params", one.params);
        }
        listed.emplace_back(std::move(action));
    }
    return listed;
}

/** A microstep as the oracle serialises it. */
boost::json::object microstep_json(const xstate::machine& machine, const xstate::microstep& step) {
    boost::json::value transitions;
    if (step.transitions.has_value()) {
        boost::json::array listed;
        for (const xstate::transition_definition* one : *step.transitions) {
            listed.emplace_back(transition_json(machine, *one));
        }
        transitions = std::move(listed);
    }
    return boost::json::object{
        {"event", xstate::to_json(step.event)},
        {"transitions", std::move(transitions)},
        {"actions", actions_json(step.actions)},
        {"snapshot", fixtures::snapshot_json(machine, step.snapshot)},
    };
}

/**
 Runs a macrostep through the cursor, one microstep a call, gives the clock
 what it returned, and returns it as the oracle records a step: its
 microsteps and the actions returned after them, or an error.

 Tip: an actor's unhandled error event fails the machine without failing
 the macrostep, as in XState, whose transition() returns that snapshot.
*/
boost::json::object run_macrostep(const xstate::machine& machine, xstate::macrostep cursor,
                                  xstate::snapshot& current, xstate::simulated_clock& clock) {
    boost::json::array microsteps;
    std::size_t returned = 0;
    std::size_t settled_on = 0;
    // The fuel a case is given: XState settled every macrostep of the cases,
    // so one that has not settled after this many has diverged from it.
    constexpr std::size_t fuel = 10000;
    while (!cursor.done()) {
        if (microsteps.size() == fuel) {
            return boost::json::object{{"unsettled_after", fuel}};
        }
        const xstate::progress made = cursor.next();
        microsteps.emplace_back(microstep_json(machine, made.step));
        returned += made.step.actions.size();
        if (made.settled) {
            ++settled_on;
        }
    }
    current = cursor.result().snapshot;
    if (current.status == xstate::status::error && current.error != xstate::errc::actor_failed) {
        return boost::json::object{{"error", true}};
    }
    for (const xstate::action& each : cursor.result().actions) {
        clock.apply(each);
    }
    // Exactly one microstep closes a macrostep (doc: #xstate-invariant-7).
    if (settled_on != 1) {
        return boost::json::object{{"settled_on", settled_on}};
    }
    boost::json::object step{{"microsteps", std::move(microsteps)}};
    const std::span<const xstate::action> all = cursor.result().actions;
    if (all.size() > returned) {
        step.emplace("trailing", actions_json(all.subspan(returned)));
    }
    return step;
}

/**
 Moves the clock and delivers each event that came due, one macrostep each,
 as the oracle's SimulatedClock delivers them.
*/
boost::json::object advance(const xstate::machine& machine, xstate::snapshot& current,
                            xstate::simulated_clock& clock, std::uint64_t milliseconds) {
    clock.increment(milliseconds);
    boost::json::array delivered;
    while (std::optional<xstate::event> due = clock.pop_due()) {
        delivered.emplace_back(
            run_macrostep(machine, xstate::begin(machine, current, *due), current, clock));
    }
    return boost::json::object{{"delivered", std::move(delivered)}};
}

/** Answers a query step, as the oracle answers it. */
boost::json::object answer(const xstate::machine& machine, const xstate::snapshot& current,
                           const boost::json::object& asked) {
    boost::json::object answered;
    if (const boost::json::value* sent = asked.if_contains("can")) {
        const xstate::result<xstate::event> happened = xstate::event_from_json(*sent);
        const xstate::result<bool> able =
            happened.has_value() ? xstate::can(machine, current, *happened) : happened.error();
        answered.emplace(
            "can", able.has_value() ? boost::json::value(*able) : boost::json::value("error"));
    }
    if (const boost::json::value* matches = asked.if_contains("matches")) {
        answered.emplace("matches", current.matches(*matches));
    }
    if (const boost::json::value* tag = asked.if_contains("has_tag")) {
        answered.emplace("has_tag", current.has_tag(tag->get_string()));
    }
    if (asked.contains("meta")) {
        answered.emplace("meta", xstate::get_meta(machine, current));
    }
    if (asked.contains("next_transitions")) {
        boost::json::array listed;
        for (const xstate::transition_definition* one :
             xstate::get_next_transitions(machine, current)) {
            listed.emplace_back(transition_json(machine, *one));
        }
        answered.emplace("next_transitions", std::move(listed));
    }
    return answered;
}

/** Runs the case's steps, returning them as the oracle records them. */
boost::json::array run_steps(const xstate::machine& machine, const boost::json::array& steps) {
    boost::json::array actual;
    xstate::snapshot current;
    xstate::simulated_clock clock;
    for (const boost::json::value& step : steps) {
        const boost::json::object& asked = step.get_object();
        if (asked.contains("start")) {
            const boost::json::value* input = asked.if_contains("input");
            actual.emplace_back(run_macrostep(
                machine,
                xstate::begin_initial(machine, input == nullptr ? boost::json::value() : *input),
                current, clock));
        } else if (const boost::json::value* sent = asked.if_contains("send")) {
            const xstate::result<xstate::event> happened = xstate::event_from_json(*sent);
            if (!happened.has_value()) {
                actual.emplace_back(boost::json::object{{"error", true}});
                continue;
            }
            actual.emplace_back(
                run_macrostep(machine, xstate::begin(machine, current, *happened), current, clock));
        } else if (const boost::json::value* resolved = asked.if_contains("resolve")) {
            const boost::json::value* context = asked.if_contains("context");
            const xstate::result<xstate::snapshot> made = xstate::resolve_state(
                machine, *resolved,
                context == nullptr ? boost::json::value(boost::json::object{}) : *context);
            if (!made.has_value()) {
                actual.emplace_back(boost::json::object{{"error", true}});
                continue;
            }
            current = *made;
            actual.emplace_back(
                boost::json::object{{"resolved", fixtures::snapshot_json(machine, current)}});
        } else if (const boost::json::value* moved = asked.if_contains("advance")) {
            // xstate's clock counts whole milliseconds, where XState's takes
            // fractions; a fractional advance differs from XState's record.
            const double milliseconds = vocabulary::number_of(*moved);
            actual.emplace_back(
                milliseconds < 0 || milliseconds != std::floor(milliseconds)
                    ? boost::json::object{{"no_whole_milliseconds", true}}
                    : advance(machine, current, clock, static_cast<std::uint64_t>(milliseconds)));
        } else if (const boost::json::value* query = asked.if_contains("query")) {
            actual.emplace_back(
                boost::json::object{{"query", answer(machine, current, query->get_object())}});
        } else {
            actual.emplace_back(boost::json::object{{"unsupported", step}});
        }
    }
    return actual;
}

/**
 A definition without the higher-order guards XState cannot write: in XState
 they are functions, which JSON.stringify leaves out.
*/
boost::json::value without_higher_order_guards(boost::json::value definition) {
    std::vector<boost::json::value*> pending{&definition};
    while (!pending.empty()) {
        boost::json::value* visited = pending.back();
        pending.pop_back();
        if (visited->is_array()) {
            for (boost::json::value& item : visited->get_array()) {
                pending.push_back(&item);
            }
            continue;
        }
        if (!visited->is_object()) {
            continue;
        }
        boost::json::object& object = visited->get_object();
        const boost::json::value* guard = object.if_contains("guard");
        if (guard != nullptr && guard->is_object()) {
            const boost::json::value* type = guard->get_object().if_contains("type");
            if (type != nullptr && type->is_string() && type->get_string().starts_with("xstate.")) {
                object.erase("guard");
            }
        }
        for (auto& member : object) {
            pending.push_back(&member.value());
        }
    }
    return definition;
}

/**
 Whether one object of a config holds what XState's definition leaves out:
 an eventless transition, a higher-order guard, a history default target, or
 a state key with a dot, which can give two states one id.
*/
bool is_lossy(const boost::json::object& object) {
    const boost::json::value* guard = object.if_contains("guard");
    const boost::json::object* guard_object =
        guard != nullptr && guard->is_object() ? &guard->get_object() : nullptr;
    const bool higher_order = guard_object != nullptr && (guard_object->contains("guards") ||
                                                          guard_object->contains("stateValue"));
    const boost::json::value* type = object.if_contains("type");
    const bool is_history = object.contains("history") || (type != nullptr && type->is_string() &&
                                                           type->get_string() == "history");
    if (object.contains("always") || higher_order || (is_history && object.contains("target"))) {
        return true;
    }
    const boost::json::value* states = object.if_contains("states");
    if (states == nullptr || !states->is_object()) {
        return false;
    }
    return std::ranges::any_of(states->get_object(),
                               [](const auto& member) { return member.key().contains('.'); });
}

/** Whether XState's definition holds all of a config. */
bool definition_is_lossless(const boost::json::value& config) {
    std::vector<const boost::json::value*> pending{&config};
    while (!pending.empty()) {
        const boost::json::value* visited = pending.back();
        pending.pop_back();
        if (visited->is_array()) {
            for (const boost::json::value& item : visited->get_array()) {
                pending.push_back(&item);
            }
            continue;
        }
        if (!visited->is_object()) {
            continue;
        }
        if (is_lossy(visited->get_object())) {
            return false;
        }
        for (const auto& member : visited->get_object()) {
            pending.push_back(&member.value());
        }
    }
    return true;
}

/**
 Runs one case and returns, for each step that differs from XState's, a
 line saying how; an empty list when every step is XState's.
*/
std::vector<std::string> differences(const boost::json::object& the_case,
                                     const boost::json::object& expected) {
    std::vector<std::string> found;
    const std::string name(the_case.at("name").get_string());
    const xstate::result<xstate::machine> machine =
        xstate::create_machine(the_case.at("machine"), vocabulary::of_case(the_case));
    if (expected.contains("create_error")) {
        if (machine.has_value()) {
            found.push_back(name + ": xstate created a machine XState refuses");
        }
        return found;
    }
    // A case where xstate refuses, at creation, what XState refuses only when
    // it runs (doc: #differences-refused).
    if (const boost::json::value* refuses = the_case.if_contains("xstate_refuses")) {
        const std::string code =
            machine.has_value() ? std::string("nothing") : machine.error().message();
        if (code != refuses->get_string()) {
            found.push_back(name + ": xstate should refuse it with " +
                            std::string(refuses->get_string()) + ", refused with " + code);
        }
        return found;
    }
    if (!machine.has_value()) {
        found.push_back(name + ": xstate refused the machine: " + machine.error().message());
        return found;
    }
    if (const boost::json::value* definition = expected.if_contains("definition")) {
        const boost::json::value written = without_higher_order_guards(xstate::to_json(*machine));
        if (!fixtures::json_equal(*definition, written)) {
            found.push_back(name +
                            ", definition\n  XState: " + boost::json::serialize(*definition) +
                            "\n  xstate: " + boost::json::serialize(written));
        }
    }
    const boost::json::array actual = run_steps(*machine, the_case.at("steps").get_array());
    // The machine loaded back from its own definition takes the same steps.
    if (definition_is_lossless(the_case.at("machine"))) {
        const boost::json::object& machine_config = the_case.at("machine").get_object();
        const boost::json::value* context = machine_config.if_contains("context");
        const xstate::result<xstate::machine> reloaded = xstate::create_machine_from_definition(
            xstate::to_json(*machine), vocabulary::of_case(the_case),
            context == nullptr ? boost::json::value(boost::json::object{}) : *context);
        if (!reloaded.has_value()) {
            found.push_back(
                name + ": the machine's own definition was refused: " + reloaded.error().message());
        } else if (!fixtures::json_equal(boost::json::value(actual),
                                         boost::json::value(run_steps(
                                             *reloaded, the_case.at("steps").get_array())))) {
            found.push_back(name + ": the machine loaded from its definition takes other steps");
        }
    }
    const boost::json::array& produced = expected.at("steps").get_array();
    if (actual.size() != produced.size()) {
        found.push_back(name + ": " + std::to_string(actual.size()) + " steps, XState took " +
                        std::to_string(produced.size()));
    }
    for (std::size_t index = 0; index < std::min(actual.size(), produced.size()); ++index) {
        if (!fixtures::json_equal(produced[index], actual[index])) {
            found.push_back(name + ", step " + std::to_string(index) +
                            "\n  XState: " + boost::json::serialize(produced[index]) +
                            "\n  xstate: " + boost::json::serialize(actual[index]));
        }
    }
    return found;
}

std::vector<fixtures::ported_case> all_cases() {
    const std::filesystem::path fixtures_root(WEBCPP_TEST_XSTATE_FIXTURES);
    return fixtures::all_cases(fixtures_root / "cases", fixtures_root / "expected");
}

}  // namespace

BOOST_AUTO_TEST_CASE(every_case_file_has_xstates_output_for_every_case) {
    const std::vector<fixtures::ported_case> all = all_cases();
    BOOST_TEST(all.size() > 300U,
               "found " << all.size() << " cases under " << WEBCPP_TEST_XSTATE_FIXTURES);
    for (const fixtures::ported_case& ported : all) {
        BOOST_TEST(
            !ported.expected.empty(),
            ported << ": no output from XState; run b2 libs/xstate/test/oracle//update-expected");
    }
}

BOOST_DATA_TEST_CASE(every_ported_case_takes_the_steps_xstate_took, data::make(all_cases()),
                     ported) {
    if (ported.expected.empty()) {
        BOOST_ERROR(
            ported << ": no output from XState; run b2 libs/xstate/test/oracle//update-expected");
        return;
    }
    for (const std::string& difference : differences(ported.the_case, ported.expected)) {
        BOOST_ERROR(difference);
    }
}

// The loader lists every case of a case file whose XState output is missing,
// with no output, and a case file that is no case file as a case of its own
// with no output, so the suite fails on either instead of skipping it.
BOOST_AUTO_TEST_CASE(a_case_file_without_xstates_output_fails_instead_of_vanishing) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("webcpp-xstate-loader-test-" + std::to_string(std::random_device{}()));
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root / "cases");
    std::filesystem::create_directories(root / "expected");
    {
        std::ofstream ported(root / "cases" / "ported.json");
        ported << R"({
            "source": "s",
            "cases": [
                {"name": "one", "machine": {}, "steps": []},
                {"name": "two", "machine": {}, "steps": []}
            ]
        })";
        std::ofstream broken(root / "cases" / "broken.json");
        broken << R"({"source": "s", "cases": 3)";
    }
    const std::vector<fixtures::ported_case> all =
        fixtures::all_cases(root / "cases", root / "expected");
    std::filesystem::remove_all(root, ignored);
    BOOST_TEST_REQUIRE(all.size() == 3U);
    for (const fixtures::ported_case& ported : all) {
        BOOST_TEST_CONTEXT(ported) {
            BOOST_TEST(ported.expected.empty());
        }
    }
    BOOST_TEST(all[0].file == "broken.json");
    BOOST_TEST(all[1].the_case.at("name").as_string() == "one");
    BOOST_TEST(all[2].the_case.at("name").as_string() == "two");
}

// An output file that is no output file pairs its cases with no output too.
BOOST_AUTO_TEST_CASE(an_output_file_that_is_no_output_file_fails_its_cases) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("webcpp-xstate-loader-output-test-" + std::to_string(std::random_device{}()));
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root / "cases");
    std::filesystem::create_directories(root / "expected");
    {
        std::ofstream ported(root / "cases" / "ported.json");
        ported << R"({
            "source": "s",
            "cases": [
                {"name": "one", "machine": {}, "steps": []}
            ]
        })";
        std::ofstream output(root / "expected" / "ported.json");
        output << R"({
            "source": "s",
            "cases": [3, {"name": 4}]
        })";
    }
    const std::vector<fixtures::ported_case> all =
        fixtures::all_cases(root / "cases", root / "expected");
    std::filesystem::remove_all(root, ignored);
    BOOST_TEST_REQUIRE(all.size() == 1U);
    BOOST_TEST(all[0].expected.empty());
}

// A case without a name, a second case under a name its file already used,
// and a case whose output has no steps each fail, named after the problem;
// an output that says XState refused the machine is complete.
BOOST_AUTO_TEST_CASE(a_malformed_case_or_output_fails_its_case) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("webcpp-xstate-loader-malformed-test-" + std::to_string(std::random_device{}()));
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root / "cases");
    std::filesystem::create_directories(root / "expected");
    {
        std::ofstream ported(root / "cases" / "ported.json");
        ported << R"({
            "source": "s",
            "cases": [
                {"machine": {}, "steps": []},
                {"name": "twice", "machine": {}, "steps": []},
                {"name": "twice", "machine": {}, "steps": []},
                {"name": "stepless", "machine": {}, "steps": []},
                {"name": "refused", "machine": {}, "steps": []}
            ]
        })";
        std::ofstream output(root / "expected" / "ported.json");
        output << R"({
            "source": "s",
            "cases": [
                {"name": "twice", "steps": []},
                {"name": "stepless", "steps": 3},
                {"name": "refused", "create_error": true}
            ]
        })";
    }
    const std::vector<fixtures::ported_case> all =
        fixtures::all_cases(root / "cases", root / "expected");
    std::filesystem::remove_all(root, ignored);
    BOOST_TEST_REQUIRE(all.size() == 5U);
    BOOST_TEST(all[0].the_case.at("name").as_string() == "(a case without a name)");
    BOOST_TEST(all[0].expected.empty());
    BOOST_TEST(!all[1].expected.empty());
    BOOST_TEST(all[2].the_case.at("name").as_string() == "(a second case named twice)");
    BOOST_TEST(all[2].expected.empty());
    BOOST_TEST(all[3].expected.empty());
    BOOST_TEST(!all[4].expected.empty());
}
