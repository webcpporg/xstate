// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const dogMachine = createMachine({
  id: 'dog',
  initial: 'asleep',
  states: {
    asleep: {
      on: { 'wakes up': 'awake' },
    },
    awake: {
      on: { 'falls asleep': 'asleep' },
    },
  },
});

let [now] = initialTransition(dogMachine);
for (const type of ['wakes up', 'wakes up', 'falls asleep']) {
  [now] = transition(dogMachine, now, { type });
  console.log(`${type} -> ${JSON.stringify(now.value)}`);
}
