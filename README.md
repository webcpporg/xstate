# xstate

xstate is a header-only port of [XState](https://stately.ai/docs/xstate)
5.33.2's state machines and actors for C++20, one of the libraries of
[webcpp](https://github.com/webcpporg/webcpp).

A machine is data: the JSON config XState's `createMachine` reads, which
`create_machine` turns into a machine at run time, with the implementations
of its actions and guards given in C++. XState's pure functions,
`initial_transition` and `transition`, compute one snapshot from another and
change nothing, so the caller holds the snapshot and passes it back with the
next event. The actor layer runs the same machines as XState's actors do, on
[xactor](https://github.com/webcpporg/xactor): each machine an actor that
receives its events, invokes and spawns children, takes input and gives
output.

The library has two layers. `<webcpp/xstate.hpp>` is the machine core, which
needs Boost's headers and Boost.JSON and nothing else.
`<webcpp/xstate/actors.hpp>` adds the actor layer, which needs xactor too.
Every name is in the namespace `webcpp::xstate`. xstate throws nothing and
reports every error as a value, so it builds with exceptions and RTTI off; it
runs natively, on wasm32-wasip2 and on wasm32-wasip3.

## A minimal example

This is [example/xstate/qs_toggle.cpp](example/xstate/qs_toggle.cpp), a
toggle machine stepped with XState's pure functions:

<!-- include::example/xstate/qs_toggle.cpp[tag=program] -->
```cpp
#include <webcpp/xstate.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace xstate = webcpp::xstate;

int main() {
    const boost::json::value config = boost::json::parse(R"({
        "id": "toggle",
        "initial": "Inactive",
        "states": {
            "Inactive": {"on": {"toggle": "Active"}},
            "Active": {"on": {"toggle": "Inactive"}}
        }
    })");
    const xstate::result<xstate::machine> toggle_machine = xstate::create_machine(config, {});
    if (!toggle_machine.has_value()) {
        std::cout << "refused: " << toggle_machine.error().message() << '\n';
        return 1;
    }

    const auto [initial_state, initial_actions] = xstate::initial_transition(*toggle_machine);
    std::cout << "Value: " << boost::json::serialize(initial_state.value) << " ("
              << initial_actions.size() << " actions)\n";

    const xstate::event toggle{.type = "toggle", .payload = {}};
    const auto [next_state, actions] = xstate::transition(*toggle_machine, initial_state, toggle);
    std::cout << "Value: " << boost::json::serialize(next_state.value) << " (" << actions.size()
              << " actions)\n";

    const auto [last_state, last_actions] = xstate::transition(*toggle_machine, next_state, toggle);
    std::cout << "Value: " << boost::json::serialize(last_state.value) << " ("
              << last_actions.size() << " actions)\n";
    return 0;
}
```

It prints:

<!-- include::example/xstate/qs_toggle.expected[] -->
```
Value: "Inactive" (0 actions)
Value: "Active" (0 actions)
Value: "Inactive" (0 actions)
```

## Building and testing

xstate is developed inside the webcpp superproject, as a Boost library is
developed inside Boost:

    git clone --recursive https://github.com/webcpporg/webcpp
    cd webcpp
    b2 libs/xstate/test libs/xstate/example

builds the tests and the examples natively, runs every example and compares
its output with the `.expected` file beside it.
`toolset=clang-wasip2 testing.launcher=wasmtime` and
`toolset=clang-wasip3 testing.launcher=wasmtime` do the same for WASI, with
the toolsets that `user-config.jam` registers against wasi-sdk. A b2 project
uses xstate through `/webcpp/xstate//xstate`, which adds xstate's headers,
xactor's, Boost's, and Boost.JSON's definitions, compiled once per variant.

## The oracle lane

XState itself is the port's oracle. Its lane, `b2 -a
libs/xstate/test/oracle//oracle`, runs the pinned XState 5.33.2 under Node,
in its development build, over every test case of the port and every twin of
an example, and fails when what XState produces is no longer what the port's
expected results and outputs hold. It needs Node and npm, besides what the
other lanes need.

## What derives from XState

xstate ports XState 5.33.2, Copyright (c) 2015 David Khourshid, distributed
under the MIT License. These parts of this repository derive from XState:

- the machine semantics, ported from XState's algorithms into
  `include/webcpp/xstate`;
- the test cases under `test/fixtures/`, transcribed from XState's tests;
- the twins under `test/oracle/twins/`, the examples' programs written for
  XState, and the examples of `example/` that the page shares with XState's
  own documentation.

XState's MIT licence and its copyright notice are in
[LICENSE-XSTATE.txt](LICENSE-XSTATE.txt), exactly as XState 5.33.2 ships
them. That adds no licence to xstate's own code, which is under the Boost
Software License alone.

## Documentation

xstate's page, with its API reference, is published at
<https://webcpporg.github.io/webcpp/libs/xstate/>; `b2 libs/xstate/doc`
builds it into `doc/html/index.html`.

## License

Distributed under the [Boost Software License, Version 1.0](LICENSE_1_0.txt).
