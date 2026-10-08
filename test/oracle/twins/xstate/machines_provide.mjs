// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, initialTransition, setup, transition } from 'xstate';

const counter = setup({
  actions: {
    increment: assign({
      count: ({ context }) => context.count + 1,
    }),
  },
}).createMachine({
  context: { count: 0 },
  on: {
    inc: { actions: 'increment' },
  },
});

const byTen = counter.provide({
  actions: {
    increment: assign({
      count: ({ context }) => context.count + 10,
    }),
  },
});

const incrementOnce = (machine) => {
  const [initial] = initialTransition(machine);
  const [next] = transition(machine, initial, { type: 'inc' });
  return next.context;
};
console.log('counter', JSON.stringify(incrementOnce(counter)));
console.log('byTen', JSON.stringify(incrementOnce(byTen)));
