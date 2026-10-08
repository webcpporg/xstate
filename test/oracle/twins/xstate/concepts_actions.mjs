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
      exit: 'stretch',
      on: { 'wakes up': { target: 'awake', actions: 'yawn' } },
    },
    awake: {
      entry: { type: 'bark', params: { times: 2 } },
    },
  },
});

const [asleep] = initialTransition(dogMachine);
const [awake, actions] = transition(dogMachine, asleep, { type: 'wakes up' });
console.log(`value: ${JSON.stringify(awake.value)}`);
for (const action of actions) {
  const params = action.params === undefined ? '' : ` ${JSON.stringify(action.params)}`;
  console.log(`the caller executes ${action.type}${params}`);
}
