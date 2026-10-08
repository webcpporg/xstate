// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

// tag::machine[]
const toggleMachine = createMachine({
  id: 'toggle',
  initial: 'Inactive',
  states: {
    Inactive: {
      on: { toggle: 'Active' },
    },
    Active: {
      on: { toggle: 'Inactive' },
    },
  },
});
// end::machine[]

const [initialState, initialActions] =
  initialTransition(toggleMachine);
console.log('Value:',
  JSON.stringify(initialState.value),
  `(${initialActions.length} actions)`);

const toggle = { type: 'toggle' };
const [nextState, actions] = transition(
  toggleMachine,
  initialState,
  toggle,
);
console.log('Value:',
  JSON.stringify(nextState.value),
  `(${actions.length} actions)`);

const [lastState, lastActions] =
  transition(
    toggleMachine,
    nextState,
    toggle,
  );
console.log('Value:',
  JSON.stringify(lastState.value),
  `(${lastActions.length} actions)`);
