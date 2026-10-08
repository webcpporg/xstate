// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine, setup, spawnChild, stopChild } from 'xstate';

/** Prints the ids of an actor's children. */
function printChildren(actor) {
  console.log(JSON.stringify(Object.keys(actor.getSnapshot().children)));
}

const childMachine = createMachine({ id: 'child' });

const parentMachine = setup({
  actors: { childMachine },
  actions: {
    spawnChild1: spawnChild('childMachine', { id: 'child-1' }),
    spawnChild2: spawnChild('childMachine', { id: 'child-2' }),
    spawnChild3: spawnChild('childMachine', { id: 'child-3' }),
    stopChild2: stopChild('child-2'),
  },
}).createMachine({
  entry: ['spawnChild1', 'spawnChild2', 'spawnChild3'],
  on: { STOP: { actions: 'stopChild2' } },
});

const parent = createActor(parentMachine);
parent.start();
printChildren(parent); // ["child-1","child-2","child-3"]

const child2 = parent.getSnapshot().children['child-2'];
parent.send({ type: 'STOP' });
printChildren(parent); // ["child-1","child-3"]

console.log('child-2: ' + child2.getSnapshot().status);
