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
    waiting: { on: { 'leave home': 'onAWalk' } },
    onAWalk: { on: { 'arrive home': 'walkComplete' } },
    walkComplete: { type: 'final' },
  },
  output: { walked: true },
});

let [now] = initialTransition(walkMachine);
for (const type of ['leave home', 'arrive home']) {
  [now] = transition(walkMachine, now, { type });
  console.log(`${type} -> ${JSON.stringify(now.value)}, ${now.status}`);
}
if (now.output !== undefined) {
  console.log(`output: ${JSON.stringify(now.output)}`);
}
