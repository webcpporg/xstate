// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { SimulatedClock, createActor, createMachine } from 'xstate';

const toggleMachine = createMachine({
  id: 'toggle',
  initial: 'Inactive',
  states: {
    Inactive: {
      on: { toggle: 'Active' },
    },
    Active: {
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
  console.log('Value:',
    JSON.stringify(snapshot.value));
});

actor.start(); // logs "Inactive"
const toggle = { type: 'toggle' };
actor.send(toggle); // logs "Active"
clock.set(1000); // logs nothing
clock.set(2000); // logs "Inactive"
