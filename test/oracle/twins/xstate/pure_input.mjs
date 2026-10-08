// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition } from 'xstate';

const machine = createMachine({
  initial: 'pending',
  context: ({ input }) => ({
    count: input.initialCount,
  }),
  states: {
    pending: {
      on: { start: { target: 'started' } },
    },
    started: {
      entry: { type: 'doSomething' },
    },
  },
});

const [initialState] = initialTransition(machine, {
  initialCount: 0,
});
console.log('value:', JSON.stringify(initialState.value));
console.log('context:', JSON.stringify(initialState.context));
console.log('status:', initialState.status);

const [withoutInput] = initialTransition(machine);
console.log(
  `without input: ${withoutInput.status}, context`,
  JSON.stringify(withoutInput.context)
);
