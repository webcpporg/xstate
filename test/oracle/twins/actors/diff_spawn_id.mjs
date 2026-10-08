// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine, setup, spawnChild } from 'xstate';

const childMachine = createMachine({ id: 'child' });

const parentMachine = setup({
  actors: { childMachine },
  actions: {
    spawnFirst: spawnChild('childMachine', { systemId: 'first' }),
    spawnSecond: spawnChild('childMachine', { systemId: 'second' }),
  },
}).createMachine({
  entry: ['spawnFirst', 'spawnSecond'],
});

const parent = createActor(parentMachine);
parent.start();
const children = parent.getSnapshot().children;
console.log(JSON.stringify(Object.keys(children)));
for (const name of ['first', 'second']) {
  const child = parent.system.get(name);
  const keyed = Object.values(children).includes(child);
  console.log(`${name}: ${child.getSnapshot().status}, keyed: ${keyed}`);
}
