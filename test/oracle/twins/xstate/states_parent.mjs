// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const machine = createMachine({
  id: 'machine',
  initial: 'parent',
  states: {
    parent: {
      initial: 'child1',
      states: {
        child1: { on: { next: 'child2' } },
        child2: {
          initial: 'grandchild1',
          states: { grandchild1: {}, grandchild2: {} },
        },
      },
      on: {
        next: '.child2.grandchild2',
        restart: '.child1',
      },
    },
  },
});

let [snapshot] = initialTransition(machine);
console.log(JSON.stringify(snapshot.value));

[snapshot] = transition(machine, snapshot, { type: 'next' }); // child1's own transition
console.log(JSON.stringify(snapshot.value));

[snapshot] = transition(machine, snapshot, { type: 'next' }); // the parent's transition
console.log(JSON.stringify(snapshot.value));

[snapshot] = transition(machine, snapshot, { type: 'restart' });
console.log(JSON.stringify(snapshot.value));
