// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const typesOf = (actions) =>
  '[' + actions.map((a) => a.type).join(', ') + ']';

const machine = createMachine({
  initial: 'pending',
  states: {
    pending: {
      on: { start: { target: 'started' } },
    },
    started: {
      entry: { type: 'doSomething' },
    },
  },
});

const [initialState, initialActions] = initialTransition(machine);
console.log('initial value:', JSON.stringify(initialState.value));
console.log('initial actions:', typesOf(initialActions));

const [nextState, actions] = transition(machine, initialState, {
  type: 'start',
});
console.log('next value:', JSON.stringify(nextState.value));
console.log('actions:', typesOf(actions));
