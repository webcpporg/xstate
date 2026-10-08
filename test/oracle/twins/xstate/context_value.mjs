// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, getInitialSnapshot, getNextSnapshot, setup } from 'xstate';

const counter = setup({
  actions: {
    // tag::functions[]
    increment: assign(({ context }) => ({ count: context.count + 1 })),
    setCount: assign(({ event }) => {
      if (!Number.isInteger(event.count)) {
        throw new Error('set needs a count');
      }
      return { count: event.count };
    }),
    // end::functions[]
  },
}).createMachine({
  id: 'counter',
  context: { count: 0 },
  on: {
    inc: { actions: 'increment' },
    set: { actions: ['increment', 'setCount'] },
  },
});

const inc = { type: 'inc' };
const first = getInitialSnapshot(counter);
const second = getNextSnapshot(counter, first, inc);
const again = getNextSnapshot(counter, first, inc);
console.log(
  JSON.stringify(first.context),
  JSON.stringify(second.context),
  JSON.stringify(again.context),
);

// tag::failure[]
const actor = createActor(counter, { snapshot: second });
actor.subscribe({ error: () => {} });
actor.start();
actor.send({ type: 'set' });
const failed = actor.getSnapshot();
console.log(failed.status, JSON.stringify(failed.context));
// end::failure[]
