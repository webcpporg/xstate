// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { SimulatedClock, createMachine, initialTransition, transition } from 'xstate';

const light = createMachine({
  id: 'light',
  initial: 'green',
  states: {
    green: { after: { 1000: 'yellow' } },
    yellow: { after: { 500: 'red' }, on: { STOP: 'red' } },
    red: {},
  },
});

const clock = new SimulatedClock();
const timers = new Map();
let [now, initialActions] = initialTransition(light);

const apply = (action) => {
  const { params } = action;
  if (
    action.type === 'xstate.raise' &&
    typeof params.delay === 'number'
  ) {
    const timer = clock.setTimeout(
      () => deliver(params.event),
      params.delay
    );
    timers.set(params.id, timer);
  } else if (action.type === 'xstate.cancel') {
    clock.clearTimeout(timers.get(params.sendId));
  }
};
const deliver = (event) => {
  const [next, actions] = transition(light, now, event);
  actions.forEach(apply);
  now = next;
  console.log(`  ${event.type} -> ${JSON.stringify(now.value)}`);
};
initialActions.forEach(apply);

for (const time of [500, 1000, 1200, 2000]) {
  console.log(`at ${time}:`);
  if (time === 1200) {
    deliver({ type: 'STOP' });
  }
  clock.set(time);
}
