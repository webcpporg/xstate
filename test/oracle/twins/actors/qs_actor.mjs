// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine } from 'xstate';

const toggleMachine = createMachine({
  id: 'toggle',
  initial: 'Inactive',
  states: {
    Inactive: { on: { toggle: 'Active' } },
    Active: { on: { toggle: 'Inactive' } },
  },
});
const toggle = { type: 'toggle' };

// tag::actor[]
const actor = createActor(toggleMachine);

actor.subscribe((snapshot) => {
  console.log('Value:',
    JSON.stringify(snapshot.value));
});

actor.start(); // logs "Inactive"
actor.send(toggle); // logs "Active"
actor.send(toggle); // logs "Inactive"

const now = actor.getSnapshot().value;
console.log('Now:', JSON.stringify(now));
// end::actor[]
