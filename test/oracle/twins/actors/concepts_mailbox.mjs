// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, sendTo, setup } from 'xstate';

const dogMachine = setup({
  actions: {
    count: assign(({ context }) => ({ fetched: context.fetched + 1 })),
  },
}).createMachine({
  id: 'dog',
  context: { fetched: 0 },
  on: { fetch: { actions: 'count' } },
});

const ownerMachine = setup({
  actors: { dog: dogMachine },
  actions: {
    throw: sendTo('dog', { type: 'fetch' }),
  },
}).createMachine({
  id: 'owner',
  invoke: { id: 'dog', src: 'dog' },
  on: { play: { actions: ['throw', 'throw'] } },
});

const ownerActor = createActor(ownerMachine).start();
const dogActor = ownerActor.getSnapshot().children.dog;
dogActor.subscribe((snapshot) => {
  console.log(`dog: ${JSON.stringify(snapshot.context)}`);
});
ownerActor.send({ type: 'play' });
