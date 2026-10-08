// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, getMicrosteps, initialTransition, setup, transition } from 'xstate';

const dogMachine = setup({
  actions: {
    bark: () => console.log('custom action: bark'),
  },
}).createMachine({
  id: 'dog',
  initial: 'asleep',
  states: {
    asleep: { on: { 'wakes up': 'stretching' } },
    stretching: { always: 'awake' },
    awake: { entry: 'bark' },
  },
});

const [asleep] = initialTransition(dogMachine);
const wakesUp = { type: 'wakes up' };
for (const [snapshot, actions] of getMicrosteps(dogMachine, asleep, wakesUp)) {
  const types = actions.map((action) => action.type).join(', ');
  console.log(`microstep: ${JSON.stringify(snapshot.value)} [${types}]`);
}
console.log(`settled: ${JSON.stringify(transition(dogMachine, asleep, wakesUp)[0].value)}`);

const dogActor = createActor(dogMachine).start();
dogActor.send(wakesUp);
console.log(`actor: ${JSON.stringify(dogActor.getSnapshot().value)}`);
