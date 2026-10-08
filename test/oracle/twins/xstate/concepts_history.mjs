// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const walkMachine = createMachine({
  id: 'walk',
  initial: 'onAWalk',
  states: {
    onAWalk: {
      initial: 'walking',
      states: {
        walking: { on: { 'speed up': 'running' } },
        running: { on: { 'slow down': 'walking' } },
        previous: { type: 'history' },
      },
      on: { 'smells something': 'sniffing' },
    },
    sniffing: {
      on: { 'carry on': 'onAWalk.previous', 'start over': 'onAWalk' },
    },
  },
});

let [now] = initialTransition(walkMachine);
for (const type of ['speed up', 'smells something', 'carry on', 'smells something', 'start over']) {
  [now] = transition(walkMachine, now, { type });
  console.log(`${type} -> ${JSON.stringify(now.value)}`);
}
