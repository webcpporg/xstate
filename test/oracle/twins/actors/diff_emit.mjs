// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, emit, setup } from 'xstate';

const emitter = setup({
  actions: {
    emitStar: emit({ type: '*' }),
  },
}).createMachine({
  on: { PING: { actions: 'emitStar' } },
});

const actor = createActor(emitter);
actor.on('*', (heard) => console.log(`heard ${heard.type}`));
actor.start();
actor.send({ type: 'PING' });
