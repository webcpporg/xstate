// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine } from 'xstate';

const pingPong = createMachine({
  initial: 'ping',
  // Without it, createActor never returns: it runs the initial macrostep.
  options: { maxIterations: 1000 },
  states: {
    ping: { always: 'pong' },
    pong: { always: 'ping' },
  },
});

const actor = createActor(pingPong);
actor.subscribe({ error: () => {} });
actor.start();
console.log('start(): ' + actor.getSnapshot().status);
actor.stop();
console.log('stop(): ' + actor.getSnapshot().status);
