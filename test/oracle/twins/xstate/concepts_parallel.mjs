// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const walkMachine = createMachine({
  id: 'onAWalk',
  type: 'parallel',
  states: {
    activity: {
      initial: 'walking',
      states: {
        walking: { on: { 'speed up': 'running', 'sees a squirrel': 'running' } },
        running: { on: { 'slow down': 'walking' } },
      },
    },
    tail: {
      initial: 'notWagging',
      states: {
        notWagging: { on: { 'sees a squirrel': 'wagging' } },
        wagging: { on: { 'calms down': 'notWagging' } },
      },
    },
  },
});

let [now] = initialTransition(walkMachine);
console.log(`initial: ${JSON.stringify(now.value)}`);
for (const type of ['sees a squirrel', 'slow down']) {
  [now] = transition(walkMachine, now, { type });
  console.log(`${type} -> ${JSON.stringify(now.value)}`);
}
