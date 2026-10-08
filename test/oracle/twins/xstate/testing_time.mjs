// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine, SimulatedClock } from 'xstate';

const lightMachine = createMachine({
  id: 'light',
  initial: 'green',
  states: {
    green: { after: { 1000: 'yellow' } },
    yellow: { after: { 1000: 'red' } },
    red: { after: { 1000: 'green' } },
  },
});

const clock = new SimulatedClock();
const actor = createActor(lightMachine, { clock }).start();
clock.increment(500);
console.log('after 500 ms: ' + JSON.stringify(actor.getSnapshot().value));
clock.increment(510);
console.log('after 1010 ms: ' + JSON.stringify(actor.getSnapshot().value));
