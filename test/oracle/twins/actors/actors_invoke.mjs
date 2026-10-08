// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine, log, setup } from 'xstate';

/** Prints the state value and the ids of the children. */
function print(actor) {
  const snapshot = actor.getSnapshot();
  console.log(JSON.stringify(snapshot.value) + ' ' + JSON.stringify(Object.keys(snapshot.children)));
}

const worker = createMachine({
  initial: 'working',
  states: {
    working: { on: { finish: 'done' } },
    done: { type: 'final' },
  },
  output: { result: 42 },
});

const parentMachine = setup({
  actors: { worker },
  actions: { note: log(({ event }) => event, 'done') },
}).createMachine({
  initial: 'waiting',
  states: {
    waiting: {
      invoke: {
        src: 'worker',
        id: 'worker',
        onDone: { target: 'finished', actions: 'note' },
      },
    },
    finished: {},
  },
});

const parent = createActor(parentMachine, {
  logger: (label, value) => console.log(label + ' ' + JSON.stringify(value)),
});
parent.start();
print(parent); // "waiting" ["worker"]

const child = parent.getSnapshot().children.worker;
child.send({ type: 'finish' });
// logs the done event onDone took
print(parent); // "finished" []

console.log('worker: ' + child.getSnapshot().status);
