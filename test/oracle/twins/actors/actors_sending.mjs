// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine, sendTo } from 'xstate';

const authServerMachine = createMachine({
  id: 'server',
  initial: 'waitingForCode',
  states: {
    waitingForCode: {
      on: {
        CODE: {
          actions: sendTo(({ event }) => event.sender, { type: 'TOKEN' }),
        },
      },
    },
  },
});

const authClientMachine = createMachine({
  id: 'client',
  initial: 'idle',
  states: {
    idle: { on: { AUTH: { target: 'authorizing' } } },
    authorizing: {
      invoke: { id: 'auth-server', src: authServerMachine },
      entry: sendTo('auth-server', ({ self }) => ({
        type: 'CODE',
        sender: self,
      })),
      on: { TOKEN: { target: 'authorized' } },
    },
    authorized: { type: 'final' },
  },
});

const client = createActor(authClientMachine);
client.subscribe((snapshot) => {
  console.log(JSON.stringify(snapshot.value));
});
client.start();
client.send({ type: 'AUTH' });
// logs "idle", "authorizing", then "authorized"
