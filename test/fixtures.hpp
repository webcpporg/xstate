// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 What xstate's two case suites share: reading the cases and XState's output
 for them, comparing JSON as JavaScript holds it, and writing a snapshot as
 the oracles write one (test/oracle/serialize.mjs).

 Tip: a number compares by value, since JavaScript holds every number as a
 double, and an object's keys in any order.
*/
#ifndef WEBCPP_TEST_XSTATE_FIXTURES_HPP
#define WEBCPP_TEST_XSTATE_FIXTURES_HPP

#include "vocabulary.hpp"

#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace webcpp::test::xstate_fixtures {

namespace xstate = webcpp::xstate;
namespace vocabulary = webcpp::test::xstate_vocabulary;

inline std::optional<boost::json::value> read_json(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    boost::system::error_code failed;
    boost::json::value parsed = boost::json::parse(text, failed);
    if (failed) {
        return std::nullopt;
    }
    return parsed;
}

/** The entry of a fixture's "cases" whose name is `name`. */
inline const boost::json::object* case_named(const boost::json::value& fixture,
                                             std::string_view name) {
    const boost::json::value* cases =
        fixture.is_object() ? fixture.get_object().if_contains("cases") : nullptr;
    if (cases == nullptr || !cases->is_array()) {
        return nullptr;
    }
    for (const boost::json::value& one : cases->get_array()) {
        const boost::json::value* named =
            one.is_object() ? one.get_object().if_contains("name") : nullptr;
        if (named != nullptr && named->is_string() && named->get_string() == name) {
            return &one.get_object();
        }
    }
    return nullptr;
}

/** Whether two numbers are the same number, as JavaScript holds them. */
inline bool same_number(const boost::json::value& left, const boost::json::value& right) {
    return vocabulary::number_of(left) == vocabulary::number_of(right);
}

using json_pairs = std::vector<std::pair<const boost::json::value*, const boost::json::value*>>;

/**
 Whether two objects hold the same keys, queueing each pair of members to
 compare.
*/
inline bool same_keys(const boost::json::object& left, const boost::json::object& right,
                      json_pairs& pending) {
    if (left.size() != right.size()) {
        return false;
    }
    for (const auto& member : left) {
        const boost::json::value* other = right.if_contains(member.key());
        if (other == nullptr) {
            return false;
        }
        pending.emplace_back(&member.value(), other);
    }
    return true;
}

/**
 Whether two arrays are as long as each other, queueing each pair of
 elements to compare.
*/
inline bool same_length(const boost::json::array& left, const boost::json::array& right,
                        json_pairs& pending) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        pending.emplace_back(&left[index], &right[index]);
    }
    return true;
}

/**
 Whether two JSON values are equal: object keys in any order, arrays in
 order, numbers by value.
*/
inline bool json_equal(const boost::json::value& expected, const boost::json::value& actual) {
    json_pairs pending{
        {&expected, &actual},
    };
    while (!pending.empty()) {
        const auto [left, right] = pending.back();
        pending.pop_back();
        bool equal = false;
        if (left->is_number() && right->is_number()) {
            equal = same_number(*left, *right);
        } else if (left->kind() != right->kind()) {
            equal = false;
        } else if (left->is_object()) {
            equal = same_keys(left->get_object(), right->get_object(), pending);
        } else if (left->is_array()) {
            equal = same_length(left->get_array(), right->get_array(), pending);
        } else {
            equal = *left == *right;
        }
        if (!equal) {
            return false;
        }
    }
    return true;
}

inline std::string_view spelled(xstate::status state) {
    switch (state) {
        case xstate::status::active: return "active";
        case xstate::status::done: return "done";
        case xstate::status::error: return "error";
    }
    return "unknown";
}

/** A snapshot as the oracle serialises it. */
inline boost::json::object snapshot_json(const xstate::machine& machine,
                                         const xstate::snapshot& taken) {
    boost::json::array tags;
    std::vector<std::string> sorted = taken.tags;
    std::ranges::sort(sorted);
    for (const std::string& tag : sorted) {
        tags.emplace_back(tag);
    }
    boost::json::array nodes;
    for (const std::size_t node : taken.nodes) {
        nodes.emplace_back(machine.node(node).id);
    }
    boost::json::object json{
        {"value", taken.value},    {"context", taken.context},  {"status", spelled(taken.status)},
        {"tags", std::move(tags)}, {"nodes", std::move(nodes)},
    };
    if (taken.output.has_value()) {
        json.emplace("output", *taken.output);
    }
    if (taken.status == xstate::status::error && taken.error_value.has_value()) {
        json.emplace("error", *taken.error_value);
    }
    if (!taken.children.empty()) {
        boost::json::object children;
        for (const auto& [id, src] : taken.children) {
            children[id] = src;
        }
        json.emplace("children", std::move(children));
    }
    return json;
}

/** One ported case, with what XState produced for it. */
struct ported_case {
    std::string file;
    boost::json::object the_case;
    boost::json::object expected;
};

inline std::ostream& operator<<(std::ostream& out, const ported_case& ported) {
    return out << ported.file << ": " << ported.the_case.at("name").get_string();
}

/** Why a value is no case: a string name, an object machine and an array of steps; none if it is
 * one. */
inline std::optional<std::string> not_a_case(const boost::json::value& one) {
    if (!one.is_object()) {
        return "(not a case)";
    }
    const boost::json::object& the_case = one.get_object();
    const boost::json::value* name = the_case.if_contains("name");
    const boost::json::value* machine = the_case.if_contains("machine");
    const boost::json::value* steps = the_case.if_contains("steps");
    if (name == nullptr || !name->is_string()) {
        return "(a case without a name)";
    }
    if (machine == nullptr || !machine->is_object() || steps == nullptr || !steps->is_array()) {
        return "(a case without a machine or steps: " + std::string(name->get_string()) + ")";
    }
    return std::nullopt;
}

/** One case named after a problem of its file, paired with no output, so that it fails. */
inline ported_case problem(const std::filesystem::path& file, std::string what) {
    return {
        .file = file.filename().string(),
        .the_case = boost::json::object{{"name", std::move(what)}},
        .expected = boost::json::object(),
    };
}

/** A case's output among a file's, when it holds the steps XState took or that it refused the
 * machine. */
inline boost::json::object output_of(const std::optional<boost::json::value>& produced,
                                     std::string_view name) {
    const boost::json::object* output =
        produced.has_value() ? case_named(*produced, name) : nullptr;
    const boost::json::value* steps = output == nullptr ? nullptr : output->if_contains("steps");
    const bool complete = output != nullptr && ((steps != nullptr && steps->is_array()) ||
                                                output->contains("create_error"));
    return complete ? *output : boost::json::object();
}

/** The cases of one case file, as all_cases lists them. */
inline void add_cases_of(const std::filesystem::path& file, const std::filesystem::path& expected,
                         std::vector<ported_case>& all) {
    const std::optional<boost::json::value> written = read_json(file.string());
    const boost::json::value* listed = written.has_value() && written->is_object()
                                           ? written->get_object().if_contains("cases")
                                           : nullptr;
    if (listed == nullptr || !listed->is_array()) {
        all.push_back(problem(file, "(not a case file)"));
        return;
    }
    const std::optional<boost::json::value> produced =
        read_json((expected / file.filename()).string());
    std::set<std::string, std::less<>> named;
    for (const boost::json::value& one : listed->get_array()) {
        if (const std::optional<std::string> why = not_a_case(one)) {
            all.push_back(problem(file, *why));
            continue;
        }
        const boost::json::object& the_case = one.get_object();
        const std::string_view name = the_case.at("name").get_string();
        if (!named.emplace(name).second) {
            all.push_back(problem(file, "(a second case named " + std::string(name) + ")"));
            continue;
        }
        all.push_back({
            .file = file.filename().string(),
            .the_case = the_case,
            .expected = output_of(produced, name),
        });
    }
}

/**
 Every case of every case file, paired with XState's output for it. A case
 with no output, its output file missing, unreadable or without steps for
 it, is paired with an empty object, which fails its test; so is a case file
 that is no case file, a value that is no case, and a second case of a file
 under a name the file already used, each as one case named after the
 problem, so that the suite fails on it instead of skipping or aborting.
*/
inline std::vector<ported_case> all_cases(const std::filesystem::path& cases,
                                          const std::filesystem::path& expected) {
    std::vector<ported_case> all;
    std::error_code failed;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(cases, failed)) {
        if (entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    for (const std::filesystem::path& file : files) {
        add_cases_of(file, expected, all);
    }
    return all;
}

}  // namespace webcpp::test::xstate_fixtures

#endif  // WEBCPP_TEST_XSTATE_FIXTURES_HPP
