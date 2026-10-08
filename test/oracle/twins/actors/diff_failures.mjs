// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, getInitialSnapshot, setup, transition } from 'xstate';

const machine = setup({
  actions: {
    count: assign(() => {
      throw new Error('count failed');
    }),
  },
}).createMachine({
  id: 'm',
  initial: 'idle',
  context: { count: 0 },
  states: {
    idle: { on: { GO: { target: 'busy', actions: 'count' } } },
    busy: {},
  },
});

try {
  transition(machine, getInitialSnapshot(machine), { type: 'GO' });
} catch (error) {
  console.log(`transition() throws: ${error.message}`);
}

const actor = createActor(machine);
actor.subscribe({ error: () => {} });
actor.start();
actor.send({ type: 'GO' });
const failed = actor.getSnapshot();
const value = JSON.stringify(failed.value);
console.log(`an actor's snapshot: ${failed.status}, ${value}, ${failed.error.message}`);
