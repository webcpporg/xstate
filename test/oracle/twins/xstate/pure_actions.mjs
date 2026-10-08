// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, initialTransition, raise, setup, transition } from 'xstate';

const machine = setup({
  actions: {
    countUser: assign({
      users: ({ context }) => context.users + 1,
    }),
    announce: raise({ type: 'STARTED' }),
  },
}).createMachine({
  initial: 'idle',
  context: { users: 0 },
  states: {
    idle: {
      on: {
        processUser: {
          target: 'processing',
          actions: [
            {
              type: 'sendEmail',
              params: { subject: 'Processing started' },
            },
            'countUser',
            'announce',
          ],
        },
      },
    },
    processing: { on: { STARTED: 'started' } },
    started: {},
  },
});

const [idle] = initialTransition(machine);
const [nextState, actions] = transition(machine, idle, {
  type: 'processUser',
  userId: '123',
  email: 'user@example.com',
});
for (const action of actions) {
  console.log(action.type, JSON.stringify(action.params));
}
console.log('value:', JSON.stringify(nextState.value));
console.log('context:', JSON.stringify(nextState.context));
