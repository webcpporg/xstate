// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { SimulatedClock, assign, createActor, createMachine } from 'xstate';

const toggleMachine = createMachine({
  id: 'toggle',
  context: { count: 0 },
  initial: 'Inactive',
  states: {
    Inactive: {
      on: { toggle: 'Active' },
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
clock.set(2000); // Inactive, count 1
actor.send(toggle); // Active, count 2
