// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getInitialMicrosteps } from 'xstate';

const typesOf = (actions) =>
  '[' + actions.map((a) => a.type).join(', ') + ']';

const machine = createMachine({
  initial: 'a',
  states: {
    a: {
      entry: 'enterA',
      always: { target: 'b', actions: 'aToB' },
    },
    b: {
      entry: 'enterB',
    },
  },
});

const microsteps = getInitialMicrosteps(machine);
for (const [snapshot, actions] of microsteps) {
  console.log(JSON.stringify(snapshot.value), typesOf(actions));
}
