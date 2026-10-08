// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const machine = createMachine({
  id: 'light',
  initial: 'green',
  context: { cycles: 0 },
  states: {
    green: { on: { TIMER: 'yellow' } },
    yellow: { on: { TIMER: 'red' } },
    red: { on: { TIMER: 'green' } },
  },
});
const event = { type: 'TIMER' };
const [currentState] = transition(
  machine,
  initialTransition(machine)[0],
  event
);

const stateToPersist = JSON.stringify(currentState);

/* Later, perhaps in another process, the persisted text is read back. */
const restoredState = machine.resolveState(
  JSON.parse(stateToPersist)
);
const [nextState, actions] = transition(
  machine,
  restoredState,
  event
);
console.log('restored:', JSON.stringify(restoredState.value));
console.log('context:', JSON.stringify(restoredState.context));
console.log('next:', JSON.stringify(nextState.value));
