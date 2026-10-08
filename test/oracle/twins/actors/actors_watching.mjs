// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, emit, log, setup } from 'xstate';

const machine = setup({
  actions: {
    greet: (_, params) => {
      console.log('action greet ' + JSON.stringify(params));
    },
    answer: emit({ type: 'pong' }),
    note: log(({ event }) => event, 'received'),
  },
}).createMachine({
  entry: { type: 'greet', params: { name: 'Ada' } },
  on: { ping: { actions: ['answer', 'note'] } },
});

const actor = createActor(machine, {
  logger: (label, value) => {
    console.log('log ' + label + ' ' + JSON.stringify(value));
  },
});
actor.on('*', (emitted) => {
  console.log('emitted ' + JSON.stringify(emitted));
});

actor.start();
// logs: action greet {"name":"Ada"}
actor.send({ type: 'ping' });
// logs: log received {"type":"ping"}
// logs: emitted {"type":"pong"}
