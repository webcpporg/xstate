#!/usr/bin/env python3
# Copyright (c) 2026 WebCpp.org
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt)

"""Counts what xstate's page says xstate holds and webcpp's own counts do not.

tools/doc/counts.py runs this script with xstate's directory as its one argument, and gives the
page each `name=<number>` line it prints as an attribute, `{n-machine-cases}`, beside the counts
of every library: the programs b2 recorded, the headers and the twins. These are xstate's own:

* the examples of each group, `n-examples-xstate` and `n-examples-actors`, the programs of
  `example/xstate/` and `example/actors/`;
* the twins of the testing chapter's programs that are tests of Node's runner, `n-twins-node-test`:
  each `testing_<name>.mjs` under `test/oracle/twins/` that imports from `node:test`;
* the cases of the fixtures, `test/fixtures/cases/` for the machine core and
  `test/fixtures/actors/cases/` for the actor layer, under the prefixes `machine` and `actor`: the
  cases and their steps, those of a file that names one of XState's test files as its source
  (`after.test.ts`), the tests the files leave out, and the cases of xstate's own files
  (`review.json`, `resolution.json`, `vocabulary.json`); of the machine cases, also the tests left
  out because the actor cases port them, and the cases a file runs through the pure functions.

A count read from a text, a file's source or reason or a twin's imports, fails when it finds
nothing, rather than let a rewording put a zero on the page; so does an own file that is missing.
A fault is written on the standard error, and the standard output holds only counts.

Usage: counts.py <library-directory>. Exit 0 printing the counts, 1 naming a fault, 2 on a usage
error.
"""

from __future__ import annotations

import json
import re
import sys
from collections.abc import Callable
from pathlib import Path
from typing import Any

NODE_TEST = re.compile(r"""from ['"]node:test['"]""")
# A case file ported from XState's tests names one as its source, `after.test.ts` or
# `invoke.test.ts (lines 1 to 414)`; the others are xstate's own, still run against XState.
PORTED = re.compile(r'^[\w.]+\.test\.ts\b')
TO_ACTOR_CASES = re.compile(r'(?<!not )ported with the actor cases')
THROUGH_PURE = 'through the pure functions'

CaseFiles = dict[str, dict[str, Any]]


class Fault(Exception):
    """A count that cannot be made, said to the user."""


def found(count: int, what: str) -> int:
    """`count`, unless it is zero: a text that no longer reads as it did."""
    if count == 0:
        raise Fault(f'counts.py: {what}')
    return count


def case_files(directory: Path) -> CaseFiles:
    """Each case file of `directory`, by name, as JSON."""
    files = sorted(directory.glob('*.json'))
    if not files:
        raise Fault(f'counts.py: no case file in {directory}')
    return {path.stem: json.loads(path.read_text(encoding='utf-8')) for path in files}


def cases_of(files: CaseFiles, keep: Callable[[dict[str, Any]], bool] | None = None
             ) -> tuple[int, int]:
    """How many cases, and steps, the files hold, or only those `keep` selects."""
    cases = [case for held in files.values() if keep is None or keep(held)
             for case in held['cases']]
    return len(cases), sum(len(case['steps']) for case in cases)


def excluded_of(files: CaseFiles, reason: re.Pattern[str] | None = None) -> int:
    """How many tests the files leave out, or only those whose reason matches."""
    return sum(1 for held in files.values() for test in held.get('excluded', [])
               if reason is None or reason.search(test['reason']))


def case_counts(prefix: str, files: CaseFiles, own: tuple[str, ...]) -> dict[str, int]:
    """The counts of one directory of cases, under `prefix`, with those of its own files."""
    cases, steps = cases_of(files)
    ported, _ = cases_of(files, lambda held: PORTED.match(held['source']) is not None)
    counted = {
        f'n-{prefix}-cases': found(cases, f'no {prefix} case'),
        f'n-{prefix}-steps': found(steps, f'no step of a {prefix} case'),
        f'n-{prefix}-cases-ported': found(
            ported, f"no {prefix} case file names a file of XState's tests as its source"),
        f'n-{prefix}-tests-excluded': found(excluded_of(files),
                                            f'no {prefix} case file leaves a test out'),
    }
    for name in own:
        if name not in files:
            raise Fault(f'counts.py: no {name}.json among the {prefix} cases')
        counted[f'n-{prefix}-cases-{name}'] = found(len(files[name]['cases']),
                                                    f'{name}.json holds no {prefix} case')
    return counted


def counts(library: Path) -> dict[str, int]:
    """Every count of xstate's own, by its attribute's name."""
    examples = library / 'example'
    twins = library / 'test/oracle/twins'
    fixtures = library / 'test/fixtures'
    counted: dict[str, int] = {}
    for group in ('xstate', 'actors'):
        counted[f'n-examples-{group}'] = found(len(list((examples / group).glob('*.cpp'))),
                                               f'no example in {examples / group}')
    counted['n-twins-node-test'] = found(
        sum(1 for path in sorted(twins.glob('*/testing_*.mjs'))
            if NODE_TEST.search(path.read_text(encoding='utf-8'))),
        f"no twin testing_<name>.mjs of {twins} imports from 'node:test'")

    machine = case_files(fixtures / 'cases')
    counted.update(case_counts('machine', machine, ('review',)))
    counted['n-machine-tests-to-actor-cases'] = found(
        excluded_of(machine, TO_ACTOR_CASES),
        'no machine case file leaves a test out as ported with the actor cases')
    counted['n-machine-cases-through-pure'] = found(
        cases_of(machine, lambda held: THROUGH_PURE in held['source'])[0],
        f'no machine case file names its source as run {THROUGH_PURE}')
    counted.update(case_counts('actor', case_files(fixtures / 'actors/cases'),
                               ('review', 'resolution', 'vocabulary')))
    return counted


def main(argv: list[str]) -> int:
    if len(argv) != 1 or not Path(argv[0]).is_dir():
        print('usage: counts.py <library-directory>', file=sys.stderr)
        return 2
    try:
        counted = counts(Path(argv[0]))
    except (Fault, OSError, KeyError, ValueError) as fault:
        print(fault if isinstance(fault, Fault) else f'counts.py: {fault!r}', file=sys.stderr)
        return 1
    for name, value in counted.items():
        print(f'{name}={value}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
