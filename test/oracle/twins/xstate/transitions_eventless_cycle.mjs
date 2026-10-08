// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getInitialSnapshot, transition } from 'xstate';

const loop = createMachine({
  id: 'loop',
  initial: 'idle',
  // XState stops a macrostep that never settles only at maxIterations.
  options: { maxIterations: 5 },
  states: {
    idle: { on: { start: 'ping' } },
    ping: { always: 'pong' },
    pong: { always: 'ping' },
  },
});

try {
  transition(loop, getInitialSnapshot(loop), { type: 'start' });
} catch (error) {
  console.log('transition throws: ' + error.message.split(':')[0]);
}
