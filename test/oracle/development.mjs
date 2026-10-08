// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// XState's own tests run its development build, which makes checks the
// production build leaves out, and xstate makes those a JSON machine can
// meet: an event of type '*' fails the macrostep that takes it from outside,
// a transition declaring a truthy `cond` is refused, a forwardTo whose target
// resolves to no actor fails its actor. So the oracles run that build, which
// Node selects by the package's "development" condition, and refuse any
// other: node --conditions=development oracle.mjs ...

import { createMachine, getInitialSnapshot, getNextSnapshot } from 'xstate';

function runsTheDevelopmentBuild() {
  const machine = createMachine({});
  try {
    getNextSnapshot(machine, getInitialSnapshot(machine), { type: '*' });
    return false;
  } catch (error) {
    return /wildcard type/.test(error.message);
  }
}

if (!runsTheDevelopmentBuild()) {
  process.stderr.write("the xstate oracles run XState's development build: node --conditions=development\n");
  process.exit(1);
}
