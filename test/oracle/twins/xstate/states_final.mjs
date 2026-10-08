// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getMicrosteps, initialTransition, transition } from 'xstate';

const coffeeMachine = createMachine({
  id: 'coffee',
  initial: 'preparation',
  states: {
    preparation: {
      initial: 'weighing',
      states: {
        weighing: { on: { weighed: 'grinding' } },
        grinding: { on: { ground: 'ready' } },
        ready: { type: 'final' },
      },
      onDone: 'brewing',
    },
    brewing: {},
  },
});

const [grinding] = transition(
  coffeeMachine,
  initialTransition(coffeeMachine)[0],
  { type: 'weighed' },
);
console.log(JSON.stringify(grinding.value));

for (const [snapshot] of getMicrosteps(coffeeMachine, grinding, {
  type: 'ground',
})) {
  console.log(`microstep: ${JSON.stringify(snapshot.value)}`);
}
