// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { SimulatedClock, createActor, createMachine } from 'xstate';

const light = createMachine({
  initial: 'green',
  states: {
    green: { after: { 100: 'yellow' } },
    yellow: {},
  },
});

const clock = new SimulatedClock();
clock.set(1000);
try {
  clock.set(10);
  console.log(`set(10): the time is ${clock.now()}`);
} catch (error) {
  console.log(`set(10) throws: ${error.message}`);
}
const actor = createActor(light, { clock });
actor.start();
for (const time of [1050, 1100]) {
  clock.set(time);
  console.log(`at ${time}: ${JSON.stringify(actor.getSnapshot().value)}`);
}
