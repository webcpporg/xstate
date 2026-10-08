// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getMicrosteps, initialTransition } from 'xstate';
import { raise } from 'xstate/actions';

const typesOf = (actions) =>
  '[' + actions.map((a) => a.type).join(', ') + ']';

const machine = createMachine({
  initial: 'first',
  states: {
    first: {
      on: {
        TRIGGER: {
          target: 'second',
          actions: raise({ type: 'RAISED' }),
        },
      },
    },
    second: {
      on: { RAISED: 'third' },
    },
    third: {
      always: 'fourth',
    },
    fourth: {},
  },
});

const [initialState] = initialTransition(machine);
const microsteps = getMicrosteps(machine, initialState, {
  type: 'TRIGGER',
});
for (const [snapshot, actions] of microsteps) {
  console.log(JSON.stringify(snapshot.value), typesOf(actions));
}
