// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const walkMachine = createMachine({
  id: 'walk',
  initial: 'waiting',
  states: {
    waiting: {
      on: { 'leave home': 'onAWalk' },
    },
    onAWalk: {
      initial: 'walking',
      states: {
        walking: { on: { 'speed up': 'running' } },
        running: { on: { 'slow down': 'walking' } },
      },
      on: { 'arrive home': 'walkComplete' },
    },
    walkComplete: {},
  },
});

let [now] = initialTransition(walkMachine);
for (const type of ['leave home', 'speed up', 'arrive home']) {
  [now] = transition(walkMachine, now, { type });
  console.log(`${type} -> ${JSON.stringify(now.value)}, on a walk: ${now.matches('onAWalk')}`);
}
