# Working on xstate

webcpp's rules apply here: read the superproject's
[AGENTS.md](../../AGENTS.md)
(<https://github.com/webcpporg/webcpp/blob/main/AGENTS.md>) first. This file
holds only what is specific to xstate.

- **The original.** xstate ports XState 5.33.2, the npm package `xstate`
  (<https://github.com/statelyai/xstate>, MIT), pinned by
  `test/oracle/package.json` and `test/oracle/package-lock.json`. The port
  follows that version: its machine semantics, the order of its actions and
  microsteps, its errors. Moving to another version of XState is a change of
  its own, with the oracle's new expected results as its evidence.
  `LICENSE-XSTATE.txt` is XState's notice, unchanged, and the README names
  what derives from XState.
- **Two layers, one boundary.** The machine core is `<webcpp/xstate.hpp>`
  and every header other than `actors.hpp` in `include/webcpp/xstate/`: machines
  read from XState's JSON config and stepped by XState's pure functions. It
  depends on Boost's headers and Boost.JSON alone, and includes nothing of
  xactor or of the actor layer. The actor layer is `actors.hpp` and
  `include/webcpp/xstate/actors/`: the same machines run as XState's actors,
  on xactor. `meta/include-boundaries.json` declares the boundary, and the
  lint fails an include that crosses it, by file and line. `build.jam`'s
  target `/webcpp/xstate//xstate` brings xactor for the actor layer, and
  Boost.JSON's definitions, `/webcpp//boost_json`, for both.
- **Errors are values.** Every operation that can fail returns `result<T>`,
  whose errors are of the category `webcpp.xstate`, except where the actor layer
  refuses a call as xactor would, with xactor's own errors, of the category
  `webcpp.xactor` (`actor_system`'s `invalid_argument`). The values of `errc`
  are fixed and never reused. xstate throws nothing of its own, so its headers
  compile without exceptions, which the lint checks. Whether a program uses
  exceptions is its user's choice, on every target.
- **How the oracle drives XState.** Under `test/oracle/`, Node runs XState's
  development build, `node --conditions=development`, which makes the checks
  XState's own tests rely on; `development.mjs` refuses any other build.
  `oracle.mjs` runs every machine case through `machine.transition` with an
  actor scope of its own, which records each action and each microstep, and
  delivers delayed events from XState's `SimulatedClock`. `actors.mjs` runs
  every actor case on XState's actors, a host actor standing for
  `fromPromise`, and writes what each actor did, step by step, without the
  order between two actors, which XState and xactor deliver differently.
  Their output is the expected results under `test/fixtures/`, which only
  `b2 libs/xstate/test/oracle//update-expected` writes.
- **The case suites.** XState's tests are ported as data: `test/fixtures/cases/`
  for the machine core and `test/fixtures/actors/cases/` for the actor layer,
  one JSON file per test file of XState's, which names its `source` and holds
  its `cases`, with the expected results beside them under `expected/`. A test
  of XState's left out of the port is listed under the file's `excluded`, with
  its reason. `cases` and `actors_cases` are the two Boost.Test suites, native
  only, which read the fixture files and register one test case per case file.
- **The examples and their twins.** `example/xstate/` holds the machines run
  through the pure functions, `example/actors/` those run as actors: the
  page's two parts. `example/Jamfile` names every program, so a removed one
  is a visible change. Each program has a twin, the same program written for
  XState, under `test/oracle/twins/` in the same group, which either agrees,
  printing the program's own output, or diverges, with XState's output
  recorded as its own `.expected` and the difference in the page's appendix;
  a program without a twin is listed in `test/oracle/twins/without-twin.txt`
  with its reason. `example/.clang-tidy` lets an example's `main` end on an
  exception of `boost::json::parse`, as Boost's own examples do.
- **The oracle lane.** `b2 -a libs/xstate/test/oracle//oracle` runs the
  twins and both case suites on XState, and needs Node and npm besides
  Boost and a toolset.
