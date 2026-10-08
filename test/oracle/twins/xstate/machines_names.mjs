// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { initialTransition, setup, transition } from 'xstate';

const access = setup({
  guards: {
    canAccess: ({ context }) => context.role === 'admin',
  },
  delays: {
    shortDelay: 250,
  },
}).createMachine({
  id: 'access',
  context: { role: 'admin' },
  initial: 'checking',
  states: {
    checking: {
      after: {
        shortDelay: {
          guard: 'canAccess',
          actions: 'trackAccess',
          target: 'allowed',
        },
      },
    },
    allowed: {},
  },
});

const [checking, entryActions] = initialTransition(access);
for (const entry of entryActions) {
  console.log(entry.type, JSON.stringify(entry.params));
}

const delayed = { type: 'xstate.after.shortDelay.access.checking' };
const [allowed, actions] = transition(access, checking, delayed);
console.log(
  [JSON.stringify(allowed.value), ...actions.map((taken) => taken.type)].join(' '),
);
