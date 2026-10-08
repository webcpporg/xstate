// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const lightMachine = createMachine({
  id: 'light',
  initial: 'green',
  states: {
    green: { on: { timer: 'yellow' } },
    yellow: { on: { timer: 'red', emergency: 'red.stop' } },
    red: {
      initial: { target: 'walk', actions: 'beep' },
      states: { walk: {}, wait: {}, stop: {} },
    },
  },
});

const [green] = initialTransition(lightMachine);
console.log(JSON.stringify(green.value));

const [yellow] = transition(lightMachine, green, { type: 'timer' });
console.log(JSON.stringify(yellow.value));

const [walk, walkActions] = transition(lightMachine, yellow, { type: 'timer' });
console.log(
  JSON.stringify(walk.value),
  JSON.stringify(walkActions.map((action) => action.type)),
);

const [stop, stopActions] = transition(lightMachine, yellow, {
  type: 'emergency',
});
console.log(
  JSON.stringify(stop.value),
  JSON.stringify(stopActions.map((action) => action.type)),
);
