// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, initialTransition, setup, transition } from 'xstate';

// tag::setup[]
const counter = setup({
  actions: {
    increment: assign({
      count: ({ context }) => context.count + 1,
    }),
    decrement: assign({
      count: ({ context }) => context.count - 1,
    }),
  },
}).createMachine({
  context: { count: 0 },
  on: {
    inc: { actions: 'increment' },
    dec: { actions: 'decrement' },
  },
});

let [now] = initialTransition(counter);
for (const type of ['inc', 'inc', 'dec']) {
  [now] = transition(counter, now, { type });
  console.log(type, JSON.stringify(now.context));
}
// end::setup[]
