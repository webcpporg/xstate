// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { SimulatedClock, assign, createActor, setup } from 'xstate';

const retryMachine = setup({
  delays: {
    timeout: ({ context }) => context.attempts * 1000
  },
  guards: {
    canRetry: ({ context }) => context.attempts < 3
  },
  actions: {
    countAttempt: assign({ attempts: ({ context }) => context.attempts + 1 })
  }
}).createMachine({
  id: 'retry',
  initial: 'attempting',
  context: { attempts: 1 },
  states: {
    attempting: {
      after: {
        timeout: [
          {
            guard: 'canRetry',
            actions: 'countAttempt',
            target: 'attempting',
            reenter: true
          },
          { target: 'failed' }
        ]
      }
    },
    failed: {}
  }
});

const clock = new SimulatedClock();
const actor = createActor(retryMachine, { clock }).start();
for (const now of [0, 1000, 2999, 3000, 6000]) {
  clock.set(now);
  const { value, context } = actor.getSnapshot();
  console.log(`at ${now}:`, JSON.stringify(value), JSON.stringify(context));
}
