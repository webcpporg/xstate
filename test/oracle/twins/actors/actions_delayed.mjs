// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { SimulatedClock, cancel, createActor, getInitialSnapshot, raise, setup, transition } from 'xstate';

const reminder = setup({
  actions: {
    scheduleNudge: raise({ type: 'nudge' }, { id: 'nudge', delay: 1000 }),
    cancelNudge: cancel('nudge'),
  },
}).createMachine({
  id: 'reminder',
  initial: 'waiting',
  states: {
    waiting: {
      on: {
        remind: { actions: 'scheduleNudge' },
        dismiss: { actions: 'cancelNudge' },
        nudge: 'nudged',
      },
    },
    nudged: {},
  },
});

const waiting = getInitialSnapshot(reminder);
for (const type of ['remind', 'dismiss']) {
  const [next, actions] = transition(reminder, waiting, { type });
  for (const action of actions) {
    console.log(`${type}: ${action.type} ${JSON.stringify(action.params)}`);
  }
}

const clock = new SimulatedClock();
const actor = createActor(reminder, { clock }).start();

actor.send({ type: 'remind' });
actor.send({ type: 'dismiss' });
clock.increment(1000);
console.log('at 1000:', JSON.stringify(actor.getSnapshot().value));

actor.send({ type: 'remind' });
clock.increment(1000);
console.log('at 2000:', JSON.stringify(actor.getSnapshot().value));
