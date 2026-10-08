// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, emit, log, setup } from 'xstate';

const order = setup({
  actions: {
    note: () => console.log('ran note'),
    tell: emit({ type: 'told' }),
    say: log('say'),
  },
  guards: {
    broken: () => {
      throw new Error('implementation_failed');
    },
  },
}).createMachine({
  id: 'order',
  initial: 'idle',
  states: {
    idle: {
      on: {
        GO: { target: 'checking', actions: ['note', 'tell', 'say'] },
      },
    },
    checking: {
      always: { target: 'done', guard: 'broken' },
    },
    done: {},
  },
});

const actor = createActor(order, {
  logger: (value) => console.log('logged:', value),
});
actor.on('told', () => console.log('heard told'));
actor.subscribe({ error: () => {} });
actor.start();
actor.send({ type: 'GO' });

const settled = actor.getSnapshot();
console.log(
  `status: ${settled.status} (${settled.error.message})`
);
console.log('value:', JSON.stringify(settled.value));
