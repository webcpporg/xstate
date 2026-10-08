// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, emit, setup } from 'xstate';

const emitter = setup({
  actions: {
    ping: emit({ type: 'PING' }),
  },
}).createMachine({
  on: { GO: { actions: 'ping' } },
});

const actor = createActor(emitter);
actor.on('*', (heard) => console.log(`the listener of every type heard ${heard.type}`));
actor.on('PING', (heard) => console.log(`the listener of PING heard ${heard.type}`));
actor.start();
actor.send({ type: 'GO' });
