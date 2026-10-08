// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, getInitialSnapshot, getMicrosteps, raise, setup } from 'xstate';

const config = {
  id: 'order',
  initial: 'cart',
  context: { items: 0 },
  states: {
    cart: {
      on: {
        add: { actions: 'count' },
        checkout: { target: 'checking' }
      }
    },
    checking: {
      always: [{ target: 'paying', guard: 'hasItems' }, { target: 'cart' }]
    },
    paying: {
      entry: ['charge', 'confirm'],
      on: { paid: 'closed' }
    },
    closed: { type: 'final' }
  }
};

const order = setup({
  actions: {
    count: assign({ items: ({ context }) => context.items + 1 }),
    confirm: raise({ type: 'paid' })
  },
  guards: {
    hasItems: ({ context }) => context.items > 0
  }
}).createMachine(config);

let now = getInitialSnapshot(order);
console.log(`initial: ${JSON.stringify(now.value)} ${JSON.stringify(now.context)} ${now.status}`);
for (const type of ['checkout', 'add', 'checkout']) {
  const microsteps = getMicrosteps(order, now, { type });
  microsteps.forEach(([snapshot, actions], index) => {
    const value = JSON.stringify(snapshot.value);
    const context = JSON.stringify(snapshot.context);
    const types = actions.map((action) => action.type).join(', ');
    const settled = index === microsteps.length - 1 ? ' settled' : '';
    console.log(`${type} #${index + 1}: ${value} ${context} ${snapshot.status} [${types}]${settled}`);
  });
  now = microsteps[microsteps.length - 1][0];
}
