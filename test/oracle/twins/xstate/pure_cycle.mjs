// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const spinner = createMachine({
  id: 'spinner',
  initial: 'idle',
  // XState bounds a macrostep only when the config says so.
  options: { maxIterations: 1000 },
  states: {
    idle: { on: { SPIN: 'left' } },
    left: { always: 'right' },
    right: { always: 'left' },
  },
});

const [idle] = initialTransition(spinner);
try {
  transition(spinner, idle, { type: 'SPIN' });
} catch (error) {
  console.log('transition throws: ' + error.message.split(':')[0]);
}
console.log('the stable state is still ' + JSON.stringify(idle.value));
