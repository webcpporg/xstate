// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Runs every actor case, ported from XState's actor tests, on actor_system
 and compares what each actor did with what XState's actors did: the events
 it processed with the snapshot each left, the events it emitted, the
 statuses of the actors that started and the host's pending requests, after
 every step.

 Tip: the comparison is per actor and never the order between two actors,
 which xactor's FIFO and XState's nested delivery decide differently
 (doc: #differences-interleaving). The cases and XState's output are under
 WEBCPP_TEST_XSTATE_FIXTURES/actors, which test/Jamfile defines.
*/

#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include "fixtures.hpp"
#include "vocabulary.hpp"

#include <webcpp/xstate/actors.hpp>

#include <boost/json.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <ostream>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace xstate = webcpp::xstate;
namespace xactor = webcpp::xactor;
namespace fixtures = webcpp::test::xstate_fixtures;
namespace vocabulary = webcpp::test::xstate_vocabulary;
namespace data = boost::unit_test::data;

std::string_view spelled(xactor::status state) {
    switch (state) {
        case xactor::status::active: return "active";
        case xactor::status::done: return "done";
        case xactor::status::error: return "error";
        case xactor::status::stopped: return "stopped";
    }
    return "unknown";
}

/** The actors a spec declares that are machines, each with its own spec. */
std::vector<const boost::json::object*> machine_actors_of(const boost::json::object& spec) {
    std::vector<const boost::json::object*> found;
    const boost::json::value* actors = spec.if_contains("actors");
    if (actors == nullptr || !actors->is_object()) {
        return found;
    }
    for (const auto& [src, actor] : actors->get_object()) {
        const boost::json::value* kind = actor.get_object().if_contains("kind");
        if (kind != nullptr && kind->is_string() && kind->get_string() == "machine") {
            found.push_back(&actor.get_object());
        }
    }
    return found;
}

/**
 The machine a spec describes, with the vocabulary, its declared actions and
 delays, and its actors: hosts, and machines already built from their own
 specs.
*/
xstate::result<xstate::machine> machine_of(
    const boost::json::object& spec,
    const std::map<const boost::json::object*, xstate::machine>& built) {
    xstate::implementations registry = vocabulary::of_case(spec);
    if (const boost::json::value* actors = spec.if_contains("actors"); actors != nullptr) {
        for (const auto& [src, actor] : actors->get_object()) {
            const auto child = built.find(&actor.get_object());
            if (child != built.end()) {
                registry.actors.insert_or_assign(std::string(src),
                                                 xstate::machine_actor{child->second});
            }
        }
    }
    return xstate::create_machine(spec.at("machine"), std::move(registry));
}

/**
 The case's root machine, every machine actor built before the machines
 that name it.

 Tip: an explicit stack follows nested actors to any depth without
 recursion; a spec is built on its second visit, when its actors are.
*/
xstate::result<xstate::machine> root_machine_of(const boost::json::object& the_case) {
    std::map<const boost::json::object*, xstate::machine> built;
    std::vector<std::pair<const boost::json::object*, bool>> pending{{&the_case, false}};
    while (!pending.empty()) {
        auto [spec, expanded] = pending.back();
        pending.pop_back();
        if (!expanded) {
            pending.emplace_back(spec, true);
            for (const boost::json::object* child : machine_actors_of(*spec)) {
                pending.emplace_back(child, false);
            }
            continue;
        }
        xstate::result<xstate::machine> made = machine_of(*spec, built);
        if (!made.has_value()) {
            return made.error();
        }
        built.insert_or_assign(spec, std::move(*made));
    }
    return built.at(&the_case);
}

/** What the inspector records, as the oracle records it. */
class recorder {
public:
    explicit recorder(xstate::actor_system& system) : system_(system) {
        const xstate::result<void> inspecting = system_.inspect(xstate::inspector{
            .started = [this](xstate::actor_ref actor) { started_[key_of(actor)] = actor; },
            .settled =
                [this](xstate::actor_ref actor, const xstate::machine& logic,
                       const xstate::event& cause, const xstate::snapshot& settled) {
                    entry(actor)["events"].as_array().push_back(boost::json::object{
                        {"event", xstate::to_json(cause)},
                        {"snapshot", fixtures::snapshot_json(logic, settled)},
                    });
                },
            .emitted =
                [this](xstate::actor_ref actor, const xstate::event& emitted) {
                    entry(actor)["emitted"].as_array().push_back(xstate::to_json(emitted));
                },
        });
        BOOST_TEST_REQUIRE(inspecting.has_value());
    }

    /** The ids of an actor and its ancestors, the root named "root". */
    [[nodiscard]] std::string path_of(xstate::actor_ref actor) const {
        std::vector<std::string> ids;
        std::optional<xstate::actor_ref> one = actor;
        while (one.has_value()) {
            const std::optional<xstate::actor_ref> parent = system_.parent_of(*one);
            ids.emplace_back(parent.has_value() ? std::string(system_.id_of(*one)) : "root");
            one = parent;
        }
        std::string path;
        for (const std::string& id : std::ranges::reverse_view(ids)) {
            if (!path.empty()) {
                path += '/';
            }
            path += id;
        }
        return path;
    }

    /**
     An actor's key, as the oracle's keyOf names it: its path, "#2", "#3"
     added for the actors created later under a path an earlier one had.

     Tip: actor references are numbered in creation order, which is the
     order XState creates them in, its resolution order.
    */
    std::string key_of(xstate::actor_ref actor) {
        while (assigned_ < actor.value) {
            ++assigned_;
            const std::string path = path_of(xstate::actor_ref{.value = assigned_});
            const int seen = ++seen_[path];
            keys_[assigned_] = seen == 1 ? path : path + "#" + std::to_string(seen);
        }
        return keys_.at(actor.value);
    }

    /**
     Requests in the order of their actors' keys, as the oracle orders them.

     Tip: no nesting of deliveries changes a key, where the order requests
     are made in, and their actors created in, depends on it.
    */
    std::vector<xstate::host_request> by_key(std::vector<xstate::host_request> requests) {
        std::ranges::sort(requests, {},
                          [this](const xstate::host_request& one) { return key_of(one.actor); });
        return requests;
    }

    [[nodiscard]] std::optional<xstate::actor_ref> started(const std::string& path) const {
        const auto found = started_.find(path);
        return found == started_.end() ? std::nullopt : std::optional(found->second);
    }

    /** What the step recorded, then a fresh record for the next one. */
    boost::json::object take_step() { return std::exchange(step_, {}); }

    [[nodiscard]] boost::json::object statuses() const {
        boost::json::object written;
        for (const auto& [path, actor] : started_) {
            written[path] = spelled(system_.status_of(actor).value());
        }
        return written;
    }

    /** The pending requests in the order their actors were created, as the oracle writes them. */
    [[nodiscard]] boost::json::array requests() {
        boost::json::array written;
        for (const xstate::host_request& request : by_key(system_.host_requests())) {
            boost::json::object one{{"actor", key_of(request.actor)}, {"src", request.src}};
            if (request.input.has_value()) {
                one.emplace("input", *request.input);
            }
            written.emplace_back(std::move(one));
        }
        return written;
    }

private:
    boost::json::object& entry(xstate::actor_ref actor) {
        boost::json::value& recorded = step_[key_of(actor)];
        if (!recorded.is_object()) {
            recorded = boost::json::object{
                {"events", boost::json::array{}},
                {"emitted", boost::json::array{}},
            };
        }
        return recorded.as_object();
    }

    xstate::actor_system& system_;
    std::map<std::string, xstate::actor_ref> started_;
    boost::json::object step_;
    // The keys of the actors created so far, by reference, and how many
    // actors each path has had.
    std::uint32_t assigned_ = 0;
    std::map<std::uint32_t, std::string> keys_;
    std::map<std::string, int> seen_;
};

/** The request a resolve or a reject step names, as the oracle names it. */
std::optional<xstate::host_request> request_named(const boost::json::object& order,
                                                  const xstate::actor_system& system,
                                                  recorder& recorded) {
    const std::vector<xstate::host_request> pending = recorded.by_key(system.host_requests());
    if (const boost::json::value* actor = order.if_contains("actor")) {
        const std::optional<xstate::actor_ref> named =
            recorded.started(std::string(actor->get_string()));
        if (!named.has_value()) {
            return std::nullopt;
        }
        // A request no longer pending is named by its actor alone.
        return xstate::host_request{
            .actor = *named,
            .parent = xstate::actor_ref(),
            .id = std::string(),
            .src = std::string(),
            .input = std::nullopt,
        };
    }
    for (const xstate::host_request& request : pending) {
        if (request.src == order.at("src").get_string()) {
            return request;
        }
    }
    return std::nullopt;
}

/** Creates the root a create or start step needs, then starts it on a start step. */
xstate::result<xstate::run_outcome> start_step(const boost::json::object& step,
                                               const xstate::machine& root_machine,
                                               xstate::actor_system& system,
                                               std::optional<xstate::actor_ref>& root) {
    // The first creates the root, with its input and systemId; a start
    // starts it, again on a started root.
    if (!root.has_value()) {
        const boost::json::value* input = step.if_contains("input");
        const boost::json::value* system_id = step.if_contains("system_id");
        const xstate::actor_options options{
            .input = input == nullptr ? boost::json::value() : *input,
            .id = std::nullopt,
            .system_id = system_id == nullptr ? std::nullopt
                                              : std::optional<std::string>(system_id->get_string()),
        };
        root = system.create_actor(root_machine, options).value();
    }
    if (!step.contains("start")) {
        return xstate::run_outcome::settled;
    }
    return system.start(*root);
}

/**
 Answers the request a resolve or a reject step names; writes "no_request"
 when xstate has none, which XState's output never holds.
*/
xstate::result<xstate::run_outcome> answer_step(const boost::json::object& step,
                                                xstate::actor_system& system, recorder& recorded,
                                                boost::json::object& written) {
    const bool resolving = step.contains("resolve");
    const boost::json::object& order = step.at(resolving ? "resolve" : "reject").get_object();
    const std::optional<xstate::host_request> request = request_named(order, system, recorded);
    if (!request.has_value()) {
        written.emplace("no_request", true);
        return xstate::run_outcome::settled;
    }
    const boost::json::value* value = order.if_contains(resolving ? "output" : "error");
    const std::optional<boost::json::value> answer =
        value == nullptr ? std::nullopt : std::optional<boost::json::value>(*value);
    const xstate::result<xstate::run_outcome> ran =
        resolving ? system.resolve(*request, answer) : system.reject(*request, answer);
    if (!ran.has_value()) {
        written.emplace("refused", true);
    }
    return ran;
}

/**
 Moves the clock by an advance step's milliseconds.

 Tip: xactor's clock counts whole milliseconds, where XState's takes
 fractions, so a fractional advance writes what XState's record never holds.
*/
xstate::result<xstate::run_outcome> advance_step(const boost::json::value& moved,
                                                 xstate::actor_system& system, std::uint64_t& now,
                                                 boost::json::object& written) {
    const double milliseconds = vocabulary::number_of(moved);
    if (milliseconds < 0 || milliseconds != std::floor(milliseconds)) {
        written.emplace("no_whole_milliseconds", true);
        return xstate::run_outcome::settled;
    }
    now += static_cast<std::uint64_t>(milliseconds);
    return system.clock_tick(now);
}

/** Runs one step; what it writes beside what the recorder holds. */
boost::json::object run_step(const boost::json::object& step, const xstate::machine& root_machine,
                             xstate::actor_system& system, recorder& recorded,
                             std::optional<xstate::actor_ref>& root, std::uint64_t& now) {
    boost::json::object written;
    xstate::result<xstate::run_outcome> ran = xstate::run_outcome::settled;
    if (step.contains("create") || step.contains("start")) {
        ran = start_step(step, root_machine, system, root);
    } else if (const boost::json::value* got = step.if_contains("get")) {
        const std::optional<xstate::actor_ref> found = system.get(got->get_string());
        written.emplace("got", found.has_value() ? boost::json::value(recorded.key_of(*found))
                                                 : boost::json::value());
    } else if (const boost::json::value* sent = step.if_contains("send")) {
        const boost::json::value* to = step.if_contains("to");
        const std::optional<xstate::actor_ref> target =
            to == nullptr ? root : recorded.started(std::string(to->get_string()));
        const xstate::result<xstate::event> made = xstate::event_from_json(*sent);
        if (!target.has_value() || !made.has_value()) {
            // XState's output never holds it, so the step differs.
            written.emplace("no_actor", to == nullptr ? boost::json::value("root") : *to);
        } else {
            ran = system.send(*target, *made);
        }
    } else if (step.contains("resolve") || step.contains("reject")) {
        ran = answer_step(step, system, recorded, written);
    } else if (const boost::json::value* moved = step.if_contains("advance")) {
        ran = advance_step(*moved, system, now, written);
    } else if (step.contains("stop") && root.has_value()) {
        if (const xstate::result<std::vector<xstate::actor_ref>> stopped = system.stop(*root);
            !stopped.has_value()) {
            // XState's stop of a root never fails, so the step differs.
            written.emplace("stop_failed", stopped.error().message());
        }
    }
    if (ran.has_value() && *ran == xstate::run_outcome::out_of_fuel) {
        written.emplace("out_of_fuel", true);
    }
    return written;
}

/** Runs a case's steps, returning them as the oracle records them. */
boost::json::array run_steps(const xstate::machine& root_machine, const boost::json::array& steps) {
    xstate::actor_system system(xactor::budgets{.fuel = 1'000'000});
    recorder recorded(system);
    std::optional<xstate::actor_ref> root;
    std::uint64_t now = 0;
    boost::json::array actual;
    for (const boost::json::value& step : steps) {
        boost::json::object written =
            run_step(step.get_object(), root_machine, system, recorded, root, now);
        written.emplace("actors", recorded.take_step());
        written.emplace("statuses", recorded.statuses());
        written.emplace("requests", recorded.requests());
        actual.emplace_back(std::move(written));
    }
    return actual;
}

/** The events and emitted events a step recorded for one actor, empty when none. */
boost::json::object of_actor(const boost::json::object& step, std::string_view path) {
    const boost::json::value* actors = step.if_contains("actors");
    const boost::json::value* recorded =
        actors == nullptr ? nullptr : actors->get_object().if_contains(path);
    if (recorded == nullptr) {
        return boost::json::object{
            {"events", boost::json::array{}},
            {"emitted", boost::json::array{}},
        };
    }
    return recorded->get_object();
}

/** How one step differs from XState's, actor by actor; empty when it does not. */
std::vector<std::string> step_differences(const boost::json::object& expected,
                                          const boost::json::object& actual) {
    std::vector<std::string> found;
    for (const char* key : {
             "statuses",
             "requests",
             "refused",
             "got",
             "no_request",
             "no_actor",
             "no_whole_milliseconds",
             "stop_failed",
         }) {
        const boost::json::value* left = expected.if_contains(key);
        const boost::json::value* right = actual.if_contains(key);
        const bool same = left == nullptr || right == nullptr ? left == right
                                                              : fixtures::json_equal(*left, *right);
        if (!same) {
            found.push_back(
                std::string(key) +
                "\n  XState: " + (left == nullptr ? "none" : boost::json::serialize(*left)) +
                "\n  xstate: " + (right == nullptr ? "none" : boost::json::serialize(*right)));
        }
    }
    std::vector<std::string> paths;
    for (const boost::json::object* side : {&expected, &actual}) {
        if (const boost::json::value* actors = side->if_contains("actors")) {
            for (const auto& member : actors->get_object()) {
                paths.emplace_back(member.key());
            }
        }
    }
    std::ranges::sort(paths);
    paths.erase(std::ranges::unique(paths).begin(), paths.end());
    for (const std::string& path : paths) {
        const boost::json::object left = of_actor(expected, path);
        const boost::json::object right = of_actor(actual, path);
        if (!fixtures::json_equal(left, right)) {
            found.push_back("actor " + path + "\n  XState: " + boost::json::serialize(left) +
                            "\n  xstate: " + boost::json::serialize(right));
        }
    }
    return found;
}

/** Runs one case and returns how each step differs from XState's. */
std::vector<std::string> differences(const boost::json::object& the_case,
                                     const boost::json::object& expected) {
    std::vector<std::string> found;
    const std::string name(the_case.at("name").get_string());
    const xstate::result<xstate::machine> root_machine = root_machine_of(the_case);
    if (!root_machine.has_value()) {
        found.push_back(name + ": xstate refused a machine: " + root_machine.error().message());
        return found;
    }
    const boost::json::array actual = run_steps(*root_machine, the_case.at("steps").get_array());
    const boost::json::array& produced = expected.at("steps").get_array();
    if (actual.size() != produced.size()) {
        found.push_back(name + ": " + std::to_string(actual.size()) + " steps, XState took " +
                        std::to_string(produced.size()));
    }
    for (std::size_t index = 0; index < std::min(actual.size(), produced.size()); ++index) {
        for (const std::string& difference :
             step_differences(produced[index].get_object(), actual[index].get_object())) {
            std::string line = name;
            line += ", step ";
            line += std::to_string(index);
            line += ", ";
            line += difference;
            found.push_back(std::move(line));
        }
    }
    return found;
}

std::vector<fixtures::ported_case> all_actor_cases() {
    const std::filesystem::path root =
        std::filesystem::path(WEBCPP_TEST_XSTATE_FIXTURES) / "actors";
    return fixtures::all_cases(root / "cases", root / "expected");
}

}  // namespace

BOOST_AUTO_TEST_CASE(every_actor_case_file_has_xstates_output_for_every_case) {
    const std::vector<fixtures::ported_case> all = all_actor_cases();
    BOOST_TEST(!all.empty(), "no actor case under " << WEBCPP_TEST_XSTATE_FIXTURES);
    for (const fixtures::ported_case& ported : all) {
        BOOST_TEST(
            !ported.expected.empty(),
            ported << ": no output from XState; run b2 libs/xstate/test/oracle//update-expected");
    }
}

BOOST_DATA_TEST_CASE(every_actor_case_does_what_xstates_actors_did, data::make(all_actor_cases()),
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

// A step that names an actor or a request xstate does not have writes so in
// its record, which XState's never holds, instead of aborting the suite.
BOOST_AUTO_TEST_CASE(a_step_naming_what_xstate_lacks_differs_instead_of_aborting) {
    xstate::implementations registry = vocabulary::of_case({});
    registry.actors.emplace("fetch", xstate::host_actor{});
    const xstate::result<xstate::machine> machine =
        xstate::create_machine(boost::json::parse(R"({"id": "root"})"), std::move(registry));
    BOOST_TEST_REQUIRE(machine.has_value());
    const boost::json::array steps = boost::json::parse(R"([
        {"start": true},
        {"send": {"type": "PING"}, "to": "root/nobody"},
        {"resolve": {"src": "fetch", "output": 1}}
    ])")
                                         .as_array();
    const boost::json::array written = run_steps(*machine, steps);
    BOOST_TEST_REQUIRE(written.size() == 3U);
    BOOST_TEST(written[1].as_object().at("no_actor") == boost::json::value("root/nobody"));
    BOOST_TEST(written[2].as_object().at("no_request") == boost::json::value(true));
}

// A stop the system refuses is written, so the step differs from XState's,
// whose stop of a root never fails, instead of passing unseen.
BOOST_AUTO_TEST_CASE(a_stop_the_system_refuses_differs) {
    const xstate::result<xstate::machine> machine =
        xstate::create_machine(boost::json::parse(R"({"id": "root"})"), vocabulary::of_case({}));
    BOOST_TEST_REQUIRE(machine.has_value());
    xstate::actor_system system(xactor::budgets{.fuel = 1'000});
    recorder recorded(system);
    std::optional<xstate::actor_ref> root = xstate::actor_ref{.value = 99};
    std::uint64_t now = 0;
    const boost::json::object step = boost::json::parse(R"({"stop": true})").as_object();
    const boost::json::object written = run_step(step, *machine, system, recorded, root, now);
    BOOST_TEST_REQUIRE(written.contains("stop_failed"));
    boost::json::object xstates = written;
    xstates.erase("stop_failed");
    BOOST_TEST(!step_differences(xstates, written).empty());
}
