// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, initialTransition, setup, transition } from 'xstate';

const dogMachine = setup({
  actions: {
    eatTreat: assign(({ context }) => ({ treats: context.treats + 1 })),
  },
}).createMachine({
  id: 'dog',
  initial: 'begging',
  context: { treats: 0 },
  states: {
    begging: {
      on: { 'gets treat': { actions: 'eatTreat' } },
    },
  },
});

let [now] = initialTransition(dogMachine);
console.log(`${JSON.stringify(now.value)} ${JSON.stringify(now.context)}`);
for (let treat = 0; treat < 2; ++treat) {
  [now] = transition(dogMachine, now, { type: 'gets treat' });
  console.log(`${JSON.stringify(now.value)} ${JSON.stringify(now.context)}`);
}
