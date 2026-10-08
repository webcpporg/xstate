// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine } from 'xstate';

const dogMachine = createMachine({
  id: 'dog',
  initial: 'asleep',
  states: {
    asleep: {
      on: { 'wakes up': 'awake' },
    },
    awake: {
      on: { 'falls asleep': 'asleep' },
    },
  },
});

const dogActor = createActor(dogMachine);
dogActor.subscribe((snapshot) => {
  console.log(`snapshot: ${JSON.stringify(snapshot.value)}`);
});
dogActor.start();
for (const type of ['wakes up', 'falls asleep']) {
  dogActor.send({ type });
}
dogActor.stop();
console.log(`status: ${dogActor.getSnapshot().status}`);
