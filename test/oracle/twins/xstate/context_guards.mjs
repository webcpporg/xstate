// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, getInitialSnapshot, log, setup, transition } from 'xstate';

const counter = setup({
  guards: {
    belowLimit: ({ context }) => context.count < context.limit,
  },
  actions: {
    add: assign(({ context }, params) => ({
      count: context.count + params.by,
    })),
    report: log(({ context }) => context.count),
  },
}).createMachine({
  id: 'counter',
  context: { count: 0, limit: 2 },
  on: {
    inc: {
      guard: 'belowLimit',
      actions: ['report', { type: 'add', params: { by: 1 } }, 'report'],
    },
  },
});

let now = getInitialSnapshot(counter);
for (let sent = 0; sent < 3; ++sent) {
  const [next, actions] = transition(counter, now, { type: 'inc' });
  const logged = actions
    .filter((action) => action.type === 'xstate.log')
    .map((action) => action.params.value);
  console.log(`${JSON.stringify(next.context)} logged ${JSON.stringify(logged)}`);
  now = next;
}
