// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, initialTransition, setup, transition } from 'xstate';

const coffeeMachine = setup({
  actions: {
    keepDose: assign({ dose: ({ event }) => event.output }),
  },
}).createMachine({
  id: 'coffee',
  context: { dose: null },
  initial: 'preparation',
  states: {
    preparation: {
      initial: 'weighing',
      states: {
        weighing: { on: { weighed: 'grinding' } },
        grinding: { on: { ground: 'ready' } },
        ready: {
          type: 'final',
          output: ({ event }) => ({ grams: event.grams }),
        },
      },
      onDone: { target: 'brewing', actions: 'keepDose' },
    },
    brewing: {},
  },
});

let [snapshot] = initialTransition(coffeeMachine);
[snapshot] = transition(coffeeMachine, snapshot, { type: 'weighed' });
[snapshot] = transition(coffeeMachine, snapshot, { type: 'ground', grams: 18 });
console.log(JSON.stringify(snapshot.value), JSON.stringify(snapshot.context));
