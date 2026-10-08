# xstate's core tests: the conversion to lightweight_test

xstate's core tests came from xstate-cpp (`main`, `cc11cec`): the four Boost.Test files of
`test/xstate/` that test the machine core, built by that repository's `suite` rule with
`test/runner.cpp`, and the two programs of `test/headers/` that are not a header compiled alone.
Three of the four files are now Boost.Core lightweight_test programs
(`<boost/core/lightweight_test.hpp>`), one per file, which `test/Jamfile` declares with
`webcpp.run` for native, wasip2 and wasip3. The fourth, `cases`, stays on Boost.Test, declared
with `webcpp.boost-test`, natively only: it reads the fixture files and registers one test case
per ported case, and a wasm target cannot read them without a preopened directory. This file
records what each old test checked and where the new one checks it, so that the conversion can be
reviewed assertion by assertion.

## The programs

| Old file | Old target | New file | New target |
| --- | --- | --- | --- |
| `test/xstate/values_test.cpp` | `xstate-values` | `test/values_test.cpp` | `values` (`webcpp.run`) |
| `test/xstate/machine_test.cpp` | `xstate-machine` | `test/machine_test.cpp` | `machine` (`webcpp.run`) |
| `test/xstate/cursor_test.cpp` | `xstate-cursor` | `test/cursor_test.cpp` | `cursor` (`webcpp.run`) |
| `test/xstate/cases_test.cpp` | `xstate-cases` | `test/cases_test.cpp` | `cases` (`webcpp.boost-test`) |
| `test/xstate/fixtures.hpp`, `vocabulary.hpp` | (headers of the suites) | `test/fixtures.hpp`, `test/vocabulary.hpp` | (headers of the suites) |
| `test/headers/core_alone.cpp` | `compile core_alone.cpp` | `test/core_alone.cpp` | `core_alone` (`webcpp.compile`) |
| `test/headers/user_aggregates.cpp` | `compile user_aggregates.cpp` | `test/user_aggregates.cpp` | `user_aggregates` (`webcpp.compile`) |
| `test/.clang-tidy` | | `test/.clang-tidy` | |

`test/require.hpp` is new: `webcpp::test::require`, as xactor's, and `webcpp::test::noted`, which
prints what a check was made for on the line after its failure.

Both builds had, and have, a second variant of each suite, `<target>-noexcept`, without
exceptions and without RTTI:
- the old `suite` rule built it with `<exception-handling>off <rtti>off` and
  `STATELY_TEST_NO_EXCEPTIONS`, and compiled Boost.Test's own `runner.cpp` with exceptions on;
- `webcpp.run` builds it natively with `<exception-handling>off <rtti>off` and
  `BOOST_NO_EXCEPTIONS`, and links `tools/throw_exception.cpp`; `webcpp.boost-test` does the same
  for the suite's own sources, and compiles the framework, `tools/boost_test_runner.cpp`, with
  exceptions on in both variants.

The fixtures' directory reaches `cases` as the define `WEBCPP_TEST_XSTATE_FIXTURES`, with
forward slashes and the whole define quoted, as the old `test/Jamfile` passed
`STATELY_XSTATE_FIXTURES`, so that a checkout whose path holds a space builds.

## What is not carried, and what replaces it

| Old | What replaces it |
| --- | --- |
| The 23 hand-written `test/headers/alone_stately_xstate*.cpp`, each a header compiled alone | The 23 tests `alone-xstate-*` of `webcpp.headers-alone xstate : ../include ;`, one per public header, generated from the headers themselves. The other 10 `alone_*.cpp` of that directory were xactor's, and moved with xactor. |
| `test/toolchain_test.cpp`, the build's promise: C++20, and, in the variant without exceptions, Boost.Config seeing neither exceptions nor RTTI | The checks of `tools/test/webcpp_jam_test.py`: `test_native_builds_declared_and_noexcept_variant` (a `-noexcept` program is compiled without `__cpp_exceptions` and without `__cpp_rtti`) and `test_boost_test_passes_natively_with_noexcept` (a Boost.Test suite's `-noexcept` sources are compiled with `-fno-exceptions`, `-fno-rtti` and `-DBOOST_NO_EXCEPTIONS`, its framework with exceptions). C++20 is the Jamroot's `<cxxstd>20`, on every program's command line. `STATELY_TEST_NO_EXCEPTIONS`, which only `toolchain_test.cpp` and the old Jamfiles used, is not carried. |
| `test/failing/`, a suite that must fail in both variants | The case `test_boost_test_failure_is_red_and_named` of `tools/test/webcpp_jam_test.py`, which runs a failing Boost.Test suite through `webcpp.boost-test` and sees each variant fail, its case and checks named. |
| `test/runner.cpp`, Boost.Test's framework of each suite | `tools/boost_test_runner.cpp`, which `webcpp.boost-test` compiles for each suite. |

## How each form was converted

| Old | New | Why |
| --- | --- | --- |
| `BOOST_AUTO_TEST_CASE(name)` | `void name()` in the file's anonymous namespace, called from `main` in the old declaration order; `main` returns `boost::report_errors()` | Boost.Test ran a file's cases in declaration order. A `static void` function would have the same internal linkage, but clang-tidy's `misc-use-anonymous-namespace` rejects it, as in xactor's conversion. |
| `BOOST_DATA_TEST_CASE(name, data::make(rows), row)` | `void name(const row_type& row)`, the old body, and `void name()`, which calls it for each row of the same array, in order | Boost.Test ran one test case per row, in the array's order. A failed check names its row on the line after it (below). A failed required check returns from the row's function: it ends that row, and the next row runs, as Boost.Test ended the row's test case and ran the next. |
| `BOOST_TEST(a == b)`, operands that print | `BOOST_TEST_EQ(a, b)` | Both print the two values on failure. |
| `BOOST_TEST((expr))` | `BOOST_TEST(expr)` | The old suite wrapped an expression in parentheses where Boost.Test could not print its operands (`status`, an optional), or where `&&` joins two checks. There are six sites: old `machine_test.cpp` lines 602 and 659, and old `cursor_test.cpp` lines 197, 302, 326 and 365. |
| `BOOST_TEST(x)` | `BOOST_TEST(x)` | |
| `BOOST_TEST(a == b, boost::test_tools::per_element())` | `BOOST_TEST_ALL_EQ(a.begin(), a.end(), b.begin(), b.end())` | Both compare the sizes and each element, and count as one assertion. In `values_test.cpp`, the vector `to_state_path` returns is first bound to a local, `path`, whose iterators are passed. |
| `BOOST_TEST_REQUIRE(x)` in a test case | `if (!BOOST_TEST(x)) { return; }`, or `BOOST_TEST_EQ` by the rows above | In Boost 1.92, `BOOST_TEST` expands to `::boost::detail::test_impl(...)`, which returns `bool`, and so does `test_with_impl` for `BOOST_TEST_EQ`. The case ends, as Boost.Test ended it. Inside a loop of a case that is not a data case (old `machine_test.cpp` lines 524 and 687, old `cursor_test.cpp` line 185), the `return` ends the whole case, as the old check did. |
| `BOOST_TEST_REQUIRE(x)` in a helper | `require(BOOST_TEST(x))`, from `test/require.hpp` | A helper cannot return from its caller's case, and a program built without exceptions cannot throw out of it. So `require` ends the program with `std::exit(boost::report_errors())`, after `BOOST_TEST` has reported the failure. This differs on the failure path only: the cases after it do not run, where Boost.Test ran them. The verdict is the same. The helpers: `parsed` (values and machine), `start` (two checks), `check_same`, `acting_on_go` and `acting_without_event` (cursor). |
| `BOOST_TEST(expr, message)`, `BOOST_TEST_REQUIRE(expr, message)` and `BOOST_TEST_CONTEXT(context) { ... }` | `noted(BOOST_TEST(expr), message parts...)`, inside an `if`-return or a `require` where the old check was required | lightweight_test takes no message, so `noted` prints it on the line after the failure, which names its file, line and function, and returns what `BOOST_TEST` returned. The check itself is unchanged, and stays not required where it was not. The sites: old `values_test.cpp` line 38; old `machine_test.cpp` lines 39, 414 to 417, 524, 525, 528, 598 to 603 and 656 to 660; old `cursor_test.cpp` lines 124 to 127, 143, 185, 268 and 367 to 369. |
| A data case's row | `noted(..., "for ", row)` on each check of the row's function | Boost.Test named the row by its index in the case's name (`name/_5`) and printed the sample (`row = ...`) as the failure's context; the row's own `operator<<`, which the old file defined for that context, now prints it. `check_same` of `cursor_test.cpp` takes the row, `stopped`, as a third parameter, to name it. |
| `BOOST_CHECK_THROW`, `STATELY_TEST_NO_EXCEPTIONS` | (none) | None of the four files uses either, so there is no `BOOST_TEST_THROWS` and no `#ifndef BOOST_NO_EXCEPTIONS`; the rule that would have converted them has nothing to convert. |
| `cases_test.cpp`'s macros | the same | It stays on Boost.Test, through `webcpp.boost-test`. Only its names and two messages change (below). |

These are the macros of each converted file, old and new. Each old `BOOST_TEST_REQUIRE` became
one `if`-return or one `require`, and each assertion stayed one assertion:

| File | Old `BOOST_TEST_REQUIRE` | Old `BOOST_TEST` | New `if (!...BOOST_TEST...)` | New `require(...BOOST_TEST...)` | New `BOOST_TEST` | New `BOOST_TEST_EQ` | New `BOOST_TEST_ALL_EQ` |
| --- | --: | --: | --: | --: | --: | --: | --: |
| `values_test.cpp` | 1 | 5 | 0 | 1 | 1 | 4 | 1 |
| `machine_test.cpp` | 16 | 23 | 15 | 1 | 25 | 14 | 0 |
| `cursor_test.cpp` | 15 | 40 | 10 | 5 | 25 | 27 | 3 |

The new `BOOST_TEST`, `_EQ` and `_ALL_EQ` columns include those inside an `if`-return or a
`require`. Of the old `BOOST_TEST`s, 1, 0 and 3 compare `per_element`.

## How the assertions were counted

Two counts were taken, and both agree, case by case. Each assertion was also paired with its
counterpart, as the end of this section describes.

- **Static**, in the source. Comments and string literals are blanked. Then every assertion
  macro is attributed to the outermost function body that holds it: a lambda counts in the
  function around it, a helper counts on its own row, and a data case's assertions are those of
  its row's function.
  - Old: `BOOST_TEST`, `BOOST_TEST_REQUIRE`, `BOOST_CHECK*`, `BOOST_REQUIRE*`, `BOOST_WARN*`
    and `BOOST_ERROR`.
  - New: `BOOST_TEST`, `BOOST_TEST_*` and `BOOST_ERROR`.

  Each file's total was cross-checked with a plain `grep -oE` of the same macros over the
  whole file: 6, 39 and 55, old and new. These totals can be derived again at any time, from
  the old files at `cc11cec` and the new ones here, with the two commands below. The split by
  case was made with a scratch script that is not kept; it can be checked against the tables by
  hand.

      grep -oE '\bBOOST_(TEST|TEST_REQUIRE|CHECK[A-Z_]*|REQUIRE[A-Z_]*|WARN[A-Z_]*|ERROR)[[:space:]]*\(' <old file> | wc -l
      grep -oE '\bBOOST_(TEST(_[A-Z_]+)?|ERROR)[[:space:]]*\(' <new file> | wc -l

- **At run time**, the assertions each case executes, including those of the helpers it calls.
  - Old: Boost.Test's own count, from `--report_level=detailed`. Each old file was built in a
    scratch directory with Apple clang 21, Boost 1.92 and the header-only Boost.Test of
    `test/runner.cpp`, in both variants.
  - New: each program was built in a scratch directory with a header forced in (`-include`),
    which wraps `BOOST_TEST`, `BOOST_TEST_EQ`, `BOOST_TEST_NE` and `BOOST_TEST_ALL_EQ` in a
    counter, and `main` printing the counter's growth across each of its calls, one per case.
  - The default build and a `-fno-exceptions -fno-rtti -DBOOST_NO_EXCEPTIONS` build gave the
    same counts.
  - The runtime counts, 1779 old and new for the three converted files, are a record made once,
    at the commit that added this file. The counting header was a scratch tool and is not kept:
    the old counts can be made again with Boost.Test's report, and the new ones only with a tool
    like it.

`cases` is the same suite on both sides: 508 test cases (4 cases and the 504 rows of
`every_ported_case_takes_the_steps_xstate_took`, one per ported case), 19 assertions in the
source and 522 at run time, old and new.

The assertions were also compared one by one. Within each file, the old and the new assertions
were paired in source order, and each was reduced to the expression it checks:
- `BOOST_TEST_EQ(a, b)` becomes `a == b`;
- `BOOST_TEST_ALL_EQ` becomes `a == b` of its two ranges;
- the outer parentheses and a message are dropped;
- whitespace and the case of a `u` suffix are ignored;
- whether the check is required (`BOOST_TEST_REQUIRE`, an `if`-return or a `require`) is kept.

Of the 100 pairs, 98 are identical, and all 100 agree on whether the check is required. The
other two are the differences listed below: the error category's name in `values_test.cpp`,
and the local `path` of `a_state_id_splits_into_its_path`.

## The tables

A helper's assertions are counted on its own row in the static count. At run time, they are
counted in the case that calls them, as Boost.Test's report counts them. A data case's rows are
the size of its dataset, the same array on both sides; each of its rows was one Boost.Test test
case, and is one call of the row's function.

### values_test.cpp

| Old case | Rows | New function | Static, old | Static, new | Run, old | Run, new |
| --- | --: | --- | --: | --: | --: | --: |
| `every_code_spells_its_own_name` | 10 | same | 2 | 2 | 20 | 20 |
| `a_state_value_matches_as_matches_state_says` | 24 | same | 1 | 1 | 72 | 72 |
| `a_state_id_splits_into_its_path` | 3 | same | 1 | 1 | 3 | 3 |
| `an_event_reads_only_with_a_string_type` | 5 | same | 1 | 1 | 15 | 15 |
| helper `parsed` | | same | 1 | 1 | in its callers | in its callers |
| **4 cases, 42 test cases** | | **4 functions** | **6** | **6** | **110** | **110** |

### machine_test.cpp

| Old case | Rows | New function | Static, old | Static, new | Run, old | Run, new |
| --- | --: | --- | --: | --: | --: | --: |
| `every_node_has_xstates_id_type_and_order` | | same | 4 | 4 | 19 | 19 |
| `a_state_id_finds_its_node_escapes_included` | 7 | same | 2 | 2 | 21 | 21 |
| `a_config_that_is_not_a_machine_is_refused` | 33 | same | 2 | 2 | 99 | 99 |
| `a_definition_that_is_not_one_is_refused_never_aborts` | 7 | same | 2 | 2 | 21 | 21 |
| `a_definition_keeps_the_machines_version` | | same | 3 | 3 | 4 | 4 |
| `a_definition_keeps_an_initial_transitions_meta_and_description` | | same | 3 | 3 | 4 | 4 |
| `a_state_value_lists_its_keys_as_javascript_enumerates_them` | | same | 4 | 4 | 6 | 6 |
| `a_truthy_cond_is_refused_and_a_falsy_one_ignored` | | same | 3 | 3 | 23 | 23 |
| `a_spawn_child_action_names_an_actor_of_the_implementations` | | same | 2 | 2 | 3 | 3 |
| `an_implementation_holding_an_empty_function_is_refused` | | same | 3 | 3 | 41 | 41 |
| `a_computed_input_or_output_names_what_it_computes` | | same | 5 | 5 | 21 | 21 |
| `only_a_spawn_child_action_the_config_names_must_name_a_known_actor` | | same | 3 | 3 | 8 | 8 |
| `a_machine_actor_holds_the_machine_it_runs` | | same | 2 | 2 | 3 | 3 |
| helper `parsed` | | same | 1 | 1 | in its callers | in its callers |
| **13 cases, 57 test cases** | | **13 functions** | **39** | **39** | **273** | **273** |

### cursor_test.cpp

| Old case | Rows | New function | Static, old | Static, new | Run, old | Run, new |
| --- | --: | --- | --: | --: | --: | --: |
| `the_macrostep_settles_on_the_microstep_that_closes_it_and_on_no_other` | | same | 4 | 4 | 13 | 13 |
| `an_event_that_selects_no_transition_settles_at_once_changing_nothing` | | same | 6 | 6 | 8 | 8 |
| `a_copy_of_a_stopped_cursor_settles_where_a_straight_run_settles` | 5 | same | 0 | 0 | 120 | 120 |
| `an_eventless_cycle_never_settles_and_only_the_caller_stops_it` | | same | 2 | 2 | 1003 | 1003 |
| `a_failing_implementation_settles_in_error_with_the_snapshot_it_began_from` | 2 | same | 7 | 7 | 18 | 18 |
| `a_failing_microstep_reports_what_it_resolved_apart_from_its_actions` | | same | 5 | 5 | 7 | 7 |
| `an_eventless_microstep_that_only_spawns_or_stops_selects_again` | 2 | same | 2 | 2 | 204 | 204 |
| `an_eventless_microstep_that_changes_nothing_settles_the_macrostep` | | same | 3 | 3 | 4 | 4 |
| `a_send_to_a_system_id_is_returned_for_its_caller_to_resolve` | | same | 4 | 4 | 5 | 5 |
| `a_spawn_whose_input_fails_settles_in_error_without_the_child` | | same | 3 | 3 | 4 | 4 |
| `a_send_to_an_invoke_of_the_entered_state_is_bound_after_that_state_s_actions` | | same | 5 | 5 | 7 | 7 |
| `the_clock_schedules_only_a_built_in_delayed_raise` | | same | 3 | 3 | 3 | 3 |
| helper `start` | | same | 2 | 2 | in its callers | in its callers |
| helper `check_same` | | same | 7 | 7 | in its callers | in its callers |
| helper `acting_on_go` | | same | 1 | 1 | in its callers | in its callers |
| helper `acting_without_event` | | same | 1 | 1 | in its callers | in its callers |
| **12 cases, 18 test cases** | | **12 functions** | **55** | **55** | **1396** | **1396** |

### cases_test.cpp

| Case | Rows | Static | Run |
| --- | --: | --: | --: |
| `every_case_file_has_xstates_output_for_every_case` | | 2 | 505 |
| `every_ported_case_takes_the_steps_xstate_took` | 504 | 2 (`BOOST_ERROR`) | 0 |
| `a_case_file_without_xstates_output_fails_instead_of_vanishing` | | 5 | 7 |
| `an_output_file_that_is_no_output_file_fails_its_cases` | | 2 | 2 |
| `a_malformed_case_or_output_fails_its_case` | | 8 | 8 |
| **5 cases, 508 test cases** | | **19** | **522** |

The same on both sides. Its rows are the cases of `test/fixtures/cases/`, which moved
unchanged; `every_ported_case_takes_the_steps_xstate_took`
counts no assertion while every case passes, since it reports only with `BOOST_ERROR`.

In total, the three converted files have 29 cases (117 Boost.Test test cases), with 100
assertions in the source and 1779 at run time, both old and new.

## Differences, case by case

No count differs, in any case or helper. These are the other differences:

- **`every_code_spells_its_own_name`** expects the error category `webcpp.xstate`, where the old
  case expected `stately.xstate`: the library's category is renamed with it.
- **`a_state_id_splits_into_its_path`** binds the vector `to_state_path` returns to a local,
  `path`, whose iterators `BOOST_TEST_ALL_EQ` compares with the row's.
- **`an_event_reads_only_with_a_string_type`** no longer passes the serialized outcome as a
  message: `BOOST_TEST_EQ` prints both values itself.
- **`with_child`** of `machine_test.cpp`, a helper the old file defined at global scope between
  two cases, is in the anonymous namespace; and the two anonymous namespaces of
  `cursor_test.cpp` are one. Every helper keeps its place between the cases.
- **A failed `BOOST_TEST_REQUIRE` in a helper** (`parsed`, `start`, `check_same`, `acting_on_go`
  and `acting_without_event`) now ends the program. Before, it ended only its case. No such check
  fails while the library is correct, and none failed on the planted defects below.
- **An exception that escapes a case**, in the builds with exceptions, ends the program. The
  converted tests reach three calls of `at` (two of `std::array::at` in
  `every_node_has_xstates_id_type_and_order`, one of `boost::json::value::at` in `start`), one
  `get_object`, and the `boost::json::parse` without an error code of `cursor_test.cpp`'s
  helpers and of three of its cases, any of which throws when what the test expects is not there.
  Boost.Test's execution monitor caught such an exception, reported it with the case's name and
  ran the remaining cases. Now it leaves `main`, and `std::terminate` ends the program. The
  verdict is the same, since the exit status is not zero. Built without exceptions, the throw was
  and is `boost::throw_exception`'s abort in both suites. None of these throws happens while the
  library is correct, and none happened on the planted defects below.
- **The messages and contexts** are printed on the line after the failure, where Boost.Test
  printed them in the failure's line or as its context.
- **`cases_test.cpp`** names the fixtures' directory `WEBCPP_TEST_XSTATE_FIXTURES`; its two
  messages for a case without XState's output name the oracle's command,
  `b2 libs/xstate/test/oracle//update-expected`, in place of `b2 test/compat//update-expected`;
  and its scratch directories are named `webcpp-xstate-loader-*`. `fixtures.hpp` and
  `vocabulary.hpp` name the oracle's scripts under `test/oracle/`, where they moved, and
  `test/oracle/vocabulary.mjs` names `test/vocabulary.hpp`. `vocabulary.hpp`'s `sum_checked`
  names its overflow test `overflows`, which clang-tidy's `readability-simplify-boolean-expr`
  asked for; the test is the same.
- **`core_alone.cpp`** checks the guards of the same 17 headers, xactor's 10 and the actor
  layer's 7, written again by its own comment's command, which now runs from the superproject's
  `libs/`. Its opening comment is a `//` comment: the lint's Doc Comment rule reads the `\n` of
  the command's `printf` as a command in a `/** */` comment.
- **The old `-noexcept` variant** reported each failed required check twice: the check, then the
  `SIGABRT` that ended the case, since Boost.Test's `execution_aborted` cannot leave code built
  without exceptions. The new one reports it once.

## Planted defects

Each defect was planted in a scratch copy of the tree, whose path holds a space, then reverted,
with every test of `test/Jamfile` that it concerns, both variants, run from scratch (`b2 -a`)
each time. The same defect was planted in a scratch copy of xstate-cpp's headers at `cc11cec`,
and the old suites were run against it, both variants.

| Defect | Planted | New tests that fail | Old tests that fail | After the revert |
| --- | --- | --- | --- | --- |
| (1) a wrong order in `node_set.hpp` | `insert` adds a node at the front, `order_.insert(order_.begin(), node)`, in place of `order_.push_back(node)` | `cases` and `cases-noexcept`: 433 of the 504 ported cases, with 884 differences from XState | The same 433 rows of `every_ported_case_takes_the_steps_xstate_took`, with the same 884 errors, in both variants | every test passes |
| (1b) a wrong document order in `machine.hpp` | `build_nodes` pushes the children of a node first to last, so that the last is built first (`children` in place of `std::ranges::reverse_view(children)`) | `machine` and `machine-noexcept`: `every_node_has_xstates_id_type_and_order` (14 failed checks, one per node and field) and `a_definition_keeps_the_machines_version` (1); `cases` and `cases-noexcept`: 353 cases, 596 differences | The same two cases, with the same 15 failed checks, and the same 353 rows with 596 errors, in both variants | every test passes |
| (2) a cursor that skips the last microstep | `look_ahead` settles when at most one microstep is left (`(enabled.empty() ? 0U : 1U) + queue_.size() <= 1U`), in place of when none is | `cursor` and `cursor-noexcept`: `the_macrostep_settles_on_the_microstep_that_closes_it_and_on_no_other`, `an_eventless_cycle_never_settles_and_only_the_caller_stops_it`, both rows of `an_eventless_microstep_that_only_spawns_or_stops_selects_again` and `an_eventless_microstep_that_changes_nothing_settles_the_macrostep`, 5 failed checks; `cases` and `cases-noexcept`: 66 cases, 84 differences | The same four cases and two rows, with the same 5 failed checks, and the same 66 rows with 84 errors, in both variants | every test passes |
| (3) a state value that drops a parallel region | `matches_state` reads a child state value of two regions or more without its last one | `values` and `values-noexcept`: four rows of `a_state_value_matches_as_matches_state_says`, those whose child holds two parallel regions at some level (the old rows `_5`, `_6`, `_7` and `_13`); `cases` and `cases-noexcept`: 4 cases | The same four rows, and the same 4 rows of `cases`, in both variants | every test passes |

The node set of defect (1) keeps the order in which its nodes were added, which no case of
`machine_test.cpp` reads: the order of the nodes a machine is built in is `machine.hpp`'s, and
the order of a state value's keys is JavaScript's. So `machine` passes on it, old and new alike,
and defect (1b) plants the wrong document order where `machine_test.cpp` checks it.

A planted include of `<webcpp/xactor/ids.hpp>` in `include/webcpp/xstate/machine.hpp` makes
`core_alone` fail to compile, with `#error "the machine core includes <webcpp/xactor/ids.hpp>"`,
and the lint fail its include boundary at the line of the include:
`libs/xstate/include/webcpp/xstate/machine.hpp:19: the machine core runs without xactor and the
actor layer`.
