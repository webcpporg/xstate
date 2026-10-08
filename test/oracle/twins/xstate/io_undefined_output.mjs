// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition } from 'xstate';

function outputOf(done) {
  if (done.status !== 'done') {
    return 'error';
  }
  return done.output === undefined ? 'none' : JSON.stringify(done.output);
}

const orderMachine = createMachine({
  id: 'order',
  context: ({ input }) => input,
  initial: 'placed',
  states: {
    placed: { type: 'final' },
  },
  output: ({ context }) => context.total,
});

for (const input of [{ total: 42 }, {}, { total: null }]) {
  const [done] = initialTransition(orderMachine, input);
  console.log(JSON.stringify(input), '->', outputOf(done));
}
