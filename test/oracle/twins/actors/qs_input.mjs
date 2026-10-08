// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { SimulatedClock, assign, createActor, createMachine } from 'xstate';

const toggleMachine = createMachine({
  id: 'toggle',
  context: ({ input }) => ({
    count: 0,
    maxCount: input.maxCount,
  }),
  initial: 'Inactive',
  states: {
    Inactive: {
      on: {
        toggle: {
          guard: ({ context }) =>
            context.count < context.maxCount,
          target: 'Active',
        },
      },
    },
    Active: {
      entry: assign({
        count: ({ context }) =>
          context.count + 1,
      }),
      on: { toggle: 'Inactive' },
      after: { 2000: 'Inactive' },
    },
  },
});

const clock = new SimulatedClock();
const actor = createActor(toggleMachine, {
  input: { maxCount: 1 },
  clock,
});

actor.subscribe((snapshot) => {
  console.log('State:',
    JSON.stringify(snapshot.value),
    JSON.stringify(snapshot.context));
});

const toggle = { type: 'toggle' };
actor.start();
actor.send(toggle); // Active, count 1
clock.set(2000); // after 2000 ms: Inactive
actor.send(toggle); // count is not below maxCount: stays
