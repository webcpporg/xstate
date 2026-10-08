// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, forwardTo, sendParent, sendTo, setup } from 'xstate';

const counter = setup({
  actions: {
    add: assign({ count: ({ context }) => context.count + 1 }),
    setCount: assign({ count: ({ event }) => event.count }),
    report: sendParent(({ context }) => ({
      type: 'counted',
      count: context.count,
    })),
  },
}).createMachine({
  id: 'counter',
  context: { count: 0 },
  on: {
    inc: { actions: ['add', 'report'] },
    set: { actions: ['setCount', 'report'] },
  },
});

const app = setup({
  actors: { counter },
  actions: {
    inc: sendTo('counter', { type: 'inc' }),
    forwardSet: forwardTo('counter'),
    keepLast: assign({ last: ({ event }) => event.count }),
  },
}).createMachine({
  id: 'app',
  context: { last: null },
  invoke: { id: 'counter', src: 'counter' },
  on: {
    click: { actions: 'inc' },
    set: { actions: 'forwardSet' },
    counted: { actions: 'keepLast' },
  },
});

const appActor = createActor(app).start();
const click = { type: 'click' };
const set = { type: 'set', count: 10 };
for (const sent of [click, click, set]) {
  appActor.send(sent);
  const parentNow = appActor.getSnapshot();
  const childNow = parentNow.children.counter.getSnapshot();
  console.log(
    `${sent.type}: app ${JSON.stringify(parentNow.context)}, ` +
      `counter ${JSON.stringify(childNow.context)}`,
  );
}
