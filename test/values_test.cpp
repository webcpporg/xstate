// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Tests xstate's values: its error codes, state values and the paths that
 name them, as XState's matchesState and toStatePath read them, and events in
 XState's flat JSON form
 (doc: #reference-errc, #reference-state-value-hpp and #reference-event-hpp).

 Tip: the matchesState rows are the expectations of XState's match.test.ts,
 one per expect().
*/

#include <webcpp/xstate.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/json.hpp>

#include <array>
#include <ostream>
#include <string>
#include <string_view>
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

struct spelling {
    xstate::errc code;
    std::string_view name;
};

std::ostream& operator<<(std::ostream& out, const spelling& row) {
    return out << row.name;
}

const std::array spellings{
    spelling{
        .code = xstate::errc::invalid_config,
        .name = "invalid_config",
    },
    spelling{
        .code = xstate::errc::unknown_target,
        .name = "unknown_target",
    },
    spelling{
        .code = xstate::errc::unknown_state,
        .name = "unknown_state",
    },
    spelling{
        .code = xstate::errc::unknown_guard,
        .name = "unknown_guard",
    },
    spelling{
        .code = xstate::errc::unknown_delay,
        .name = "unknown_delay",
    },
    spelling{
        .code = xstate::errc::implementation_failed,
        .name = "implementation_failed",
    },
    spelling{
        .code = xstate::errc::invalid_event,
        .name = "invalid_event",
    },
    spelling{
        .code = xstate::errc::actor_failed,
        .name = "actor_failed",
    },
    spelling{
        .code = xstate::errc::unknown_actor,
        .name = "unknown_actor",
    },
    spelling{
        .code = xstate::errc::system_id_taken,
        .name = "system_id_taken",
    },
};

struct match_row {
    std::string_view parent;
    std::string_view child;
    bool matches;
};

std::ostream& operator<<(std::ostream& out, const match_row& row) {
    return out << "matchesState(" << row.parent << ", " << row.child << ")";
}

const std::array match_rows{
    match_row{
        .parent = R"("a")",
        .child = R"("a")",
        .matches = true,
    },
    match_row{
        .parent = R"("b.b1")",
        .child = R"("b.b1")",
        .matches = true,
    },
    match_row{
        .parent = R"("B.bar")",
        .child = R"({"A": "foo"})",
        .matches = false,
    },
    match_row{
        .parent = R"({"a": "b"})",
        .child = R"({"a": "b"})",
        .matches = true,
    },
    match_row{
        .parent = R"({"a": {"b": "c"}})",
        .child = R"({"a": {"b": "c"}})",
        .matches = true,
    },
    match_row{
        .parent = R"({"a": {"b1": "foo", "b2": "bar"}})",
        .child = R"({"a": {"b1": "foo", "b2": "bar"}})",
        .matches = true,
    },
    match_row{
        .parent = R"({"a": {"b1": "foo", "b2": "bar"}, "b": {"b3": "baz", "b4": "quo"}})",
        .child = R"({"a": {"b1": "foo", "b2": "bar"}, "b": {"b3": "baz", "b4": "quo"}})",
        .matches = true,
    },
    match_row{
        .parent = R"({"a": "foo", "b": "bar"})",
        .child = R"({"a": "foo", "b": "bar"})",
        .matches = true,
    },
    match_row{
        .parent = R"("b")",
        .child = R"("b.b1")",
        .matches = true,
    },
    match_row{
        .parent = R"("foo.bar")",
        .child = R"("foo.bar.baz.quo")",
        .matches = true,
    },
    match_row{
        .parent = R"("b")",
        .child = R"({"b": "b1"})",
        .matches = true,
    },
    match_row{
        .parent = R"({"foo": "bar"})",
        .child = R"({"foo": {"bar": {"baz": "quo"}}})",
        .matches = true,
    },
    match_row{
        .parent = R"("b")",
        .child = R"({"b": "b1", "c": "c1"})",
        .matches = true,
    },
    match_row{
        .parent = R"({"foo": "bar", "fooAgain": "barAgain"})",
        .child = R"({
            "foo": {"bar": {"baz": "quo"}},
            "fooAgain": {"barAgain": "baz"}
        })",
        .matches = true,
    },
    match_row{
        .parent = R"("a")",
        .child = R"("b")",
        .matches = false,
    },
    match_row{
        .parent = R"("a.a1")",
        .child = R"("b.b1")",
        .matches = false,
    },
    match_row{
        .parent = R"("a.b.c")",
        .child = R"("a.b")",
        .matches = false,
    },
    match_row{
        .parent = R"({"a": {"b": {"c": "d"}}})",
        .child = R"({"a": "b"})",
        .matches = false,
    },
    match_row{
        .parent = R"({"a": "a1"})",
        .child = R"({"b": "b1"})",
        .matches = false,
    },
    match_row{
        .parent = R"("a")",
        .child = R"("b.b1")",
        .matches = false,
    },
    match_row{
        .parent = R"("foo.false.baz")",
        .child = R"("foo.bar.baz.quo")",
        .matches = false,
    },
    match_row{
        .parent = R"("a")",
        .child = R"({"b": "b1"})",
        .matches = false,
    },
    match_row{
        .parent = R"({"foo": {"false": "baz"}})",
        .child = R"({"foo": {"bar": {"baz": "quo"}}})",
        .matches = false,
    },
    match_row{
        .parent = R"("a.b.c")",
        .child = R"({"a": {"b": "c"}})",
        .matches = true,
    },
};

struct path_row {
    std::string_view id;
    std::vector<std::string> path;
};

std::ostream& operator<<(std::ostream& out, const path_row& row) {
    return out << row.id;
}

const std::array path_rows{
    path_row{
        .id = "a",
        .path = {"a"},
    },
    path_row{
        .id = "a.b.c",
        .path = {"a", "b", "c"},
    },
    path_row{
        .id = R"(a\.b.c)",
        .path = {"a.b", "c"},
    },
};

struct event_row {
    std::string_view json;
    std::string_view outcome;
};

std::ostream& operator<<(std::ostream& out, const event_row& row) {
    return out << row.json;
}

// An outcome is the event as XState writes it, or the error it is refused with.
const std::array event_rows{
    event_row{
        .json = R"({"type": "NEXT", "count": 2})",
        .outcome = R"({"type": "NEXT", "count": 2})",
    },
    event_row{
        .json = R"({"count": 2})",
        .outcome = R"("invalid_event")",
    },
    // Read like any other: a macrostep that takes it from outside refuses it.
    event_row{
        .json = R"({"type": "*"})",
        .outcome = R"({"type": "*"})",
    },
    event_row{
        .json = R"({"type": 3})",
        .outcome = R"("invalid_event")",
    },
    event_row{
        .json = R"("NEXT")",
        .outcome = R"("invalid_event")",
    },
};

// Each data case of the old suite is a function of one row, called for every row of the same
// data, in order: a failed check names its row on the line after it.

void every_code_spells_its_own_name(const spelling& row) {
    const boost::system::error_code made = xstate::make_error_code(row.code);
    noted(BOOST_TEST_EQ(std::string(made.category().name()), "webcpp.xstate"), "for ", row);
    noted(BOOST_TEST_EQ(made.message(), row.name), "for ", row);
}

void every_code_spells_its_own_name() {
    for (const spelling& row : spellings) {
        every_code_spells_its_own_name(row);
    }
}

void a_state_value_matches_as_matches_state_says(const match_row& row) {
    noted(BOOST_TEST_EQ(xstate::matches_state(parsed(row.parent), parsed(row.child)), row.matches),
          "for ", row);
}

void a_state_value_matches_as_matches_state_says() {
    for (const match_row& row : match_rows) {
        a_state_value_matches_as_matches_state_says(row);
    }
}

void a_state_id_splits_into_its_path(const path_row& row) {
    const std::vector<std::string> path = xstate::to_state_path(row.id);
    noted(BOOST_TEST_ALL_EQ(path.begin(), path.end(), row.path.begin(), row.path.end()), "for ",
          row);
}

void a_state_id_splits_into_its_path() {
    for (const path_row& row : path_rows) {
        a_state_id_splits_into_its_path(row);
    }
}

void an_event_reads_only_with_a_string_type(const event_row& row) {
    const xstate::result<xstate::event> read = xstate::event_from_json(parsed(row.json));
    const boost::json::value outcome = read.has_value()
                                           ? boost::json::value(xstate::to_json(*read))
                                           : boost::json::value(read.error().message());
    noted(BOOST_TEST_EQ(outcome, parsed(row.outcome)), "for ", row);
}

void an_event_reads_only_with_a_string_type() {
    for (const event_row& row : event_rows) {
        an_event_reads_only_with_a_string_type(row);
    }
}

}  // namespace

int main() {
    every_code_spells_its_own_name();
    a_state_value_matches_as_matches_state_says();
    a_state_id_splits_into_its_path();
    an_event_reads_only_with_a_string_type();
    return boost::report_errors();
}
