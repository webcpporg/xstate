// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { SimulatedClock, assign, cancel, createActor, sendTo, setup } from 'xstate';

/** Prints the count in the context of the invoked someActor. */
function printCount(actor) {
  const child = actor.getSnapshot().children.someActor;
  console.log(`count ${child.getSnapshot().context.count}`);
}

const someActor = setup({
  actions: { count: assign({ count: ({ context }) => context.count + 1 }) },
}).createMachine({
  context: { count: 0 },
  on: { someEvent: { actions: 'count' } },
});

const machine = setup({
  actors: { someActor },
}).createMachine({
  invoke: { id: 'someActor', src: 'someActor' },
  on: {
    event: {
      actions: sendTo(
        'someActor',
        { type: 'someEvent' },
        { id: 'someId', delay: 1000 },
      ),
    },
    cancelEvent: { actions: cancel('someId') },
  },
});

const clock = new SimulatedClock();
const actor = createActor(machine, { clock });
actor.start();
// At 0 ms: armed for 1000 ms, then cancelled.
actor.send({ type: 'event' });
actor.send({ type: 'cancelEvent' });
clock.set(1000);
printCount(actor); // count 0

// At 1000 ms: armed for 2000 ms.
actor.send({ type: 'event' });
clock.set(1999);
printCount(actor); // count 0
clock.set(2000);
printCount(actor); // count 1
