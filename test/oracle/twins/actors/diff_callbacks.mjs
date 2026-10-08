// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine } from 'xstate';

const steps = createMachine({
  initial: 'a',
  states: {
    a: { on: { NEXT: 'b' } },
    b: { on: { NEXT: 'c' } },
    c: {},
  },
});

const actor = createActor(steps);
actor.subscribe((snapshot) => {
  console.log(`heard ${JSON.stringify(snapshot.value)}`);
  if (snapshot.value === 'b') {
    actor.send({ type: 'NEXT' });
    console.log('  the listener sent NEXT');
  }
});
actor.start();
actor.send({ type: 'NEXT' });
console.log(`after the host's send: ${JSON.stringify(actor.getSnapshot().value)}`);
