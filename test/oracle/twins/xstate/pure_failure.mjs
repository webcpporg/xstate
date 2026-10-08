// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, setup } from 'xstate';

const upload = setup({
  actions: {
    notify: () => console.log('ran notify'),
    progress: () => console.log('ran progress'),
    countAttempt: assign({
      attempts: ({ context }) => context.attempts + 1,
    }),
    fail: assign(() => {
      throw new Error('implementation_failed');
    }),
  },
}).createMachine({
  id: 'upload',
  initial: 'idle',
  context: { attempts: 0 },
  states: {
    idle: {
      on: {
        UPLOAD: { target: 'uploading', actions: 'notify' },
      },
    },
    uploading: {
      always: {
        target: 'done',
        actions: ['progress', 'countAttempt', 'fail'],
      },
    },
    done: {},
  },
});

const actor = createActor(upload);
actor.subscribe({ error: () => {} });
actor.start();
actor.send({ type: 'UPLOAD' });

const settled = actor.getSnapshot();
console.log(
  `status: ${settled.status} (${settled.error.message})`
);
console.log('value:', JSON.stringify(settled.value));
console.log('context:', JSON.stringify(settled.context));
