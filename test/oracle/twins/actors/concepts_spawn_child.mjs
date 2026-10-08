// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine, setup, spawnChild } from 'xstate';

const dogMachine = createMachine({
  id: 'dog',
  initial: 'sniffing',
  states: {
    sniffing: { on: { 'finds a stick': 'fetched' } },
    fetched: { type: 'final' },
  },
  output: { brought: 'a stick' },
});

/** Prints an actor's state value and status, and its output once it has one. */
function print(label, snapshot) {
  const output = snapshot.output === undefined ? '' : ', output ' + JSON.stringify(snapshot.output);
  console.log(`${label}: ${JSON.stringify(snapshot.value)}, ${snapshot.status}${output}`);
}
const ownerMachine = setup({
  actors: { dog: dogMachine },
  actions: {
    adopt: spawnChild('dog', { id: 'dog' }),
  },
}).createMachine({
  id: 'owner',
  initial: 'walking',
  states: {
    walking: {
      entry: 'adopt',
      on: { 'xstate.done.actor.dog': 'home' },
    },
    home: { type: 'final' },
  },
});

const ownerActor = createActor(ownerMachine).start();
const dogActor = ownerActor.getSnapshot().children.dog;
print('owner', ownerActor.getSnapshot());

dogActor.send({ type: 'finds a stick' });
print('owner', ownerActor.getSnapshot());
print('dog', dogActor.getSnapshot());
