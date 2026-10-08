// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { SimulatedClock, createActor, initialTransition, setup, transition } from 'xstate';

const gameMachine = setup({
  actions: {
    logThatYouGotTimedOut: () => console.log('logThatYouGotTimedOut'),
    logSuccess: () => console.log('logSuccess')
  }
}).createMachine({
  id: 'game',
  initial: 'waitingForButtonPush',
  states: {
    waitingForButtonPush: {
      after: {
        5000: { target: 'timedOut', actions: 'logThatYouGotTimedOut' }
      },
      on: {
        PUSH_BUTTON: { actions: 'logSuccess', target: 'success' }
      }
    },
    success: {},
    timedOut: {}
  }
});

/** Prints a macrostep's actions, one a line: its type, and its params when it has some. */
function printActions(macrostep, actions) {
  console.log(`${macrostep}:`);
  for (const { type, params } of actions) {
    console.log(params === undefined ? `  ${type}` : `  ${type} ${JSON.stringify(params)}`);
  }
}

const [waiting, entered] = initialTransition(gameMachine);
printActions('initial macrostep', entered);
printActions('PUSH_BUTTON', transition(gameMachine, waiting, { type: 'PUSH_BUTTON' })[1]);

/** A player who never pushes. */
{
  const clock = new SimulatedClock();
  const actor = createActor(gameMachine, { clock }).start();
  console.log('at 0:', JSON.stringify(actor.getSnapshot().value));
  for (const time of [4999, 5000]) {
    clock.set(time);
    console.log(`at ${time}:`, JSON.stringify(actor.getSnapshot().value));
  }
}

/** A player who pushes the button at 1000. */
{
  const clock = new SimulatedClock();
  const actor = createActor(gameMachine, { clock }).start();
  console.log('at 0:', JSON.stringify(actor.getSnapshot().value));
  clock.set(1000);
  actor.send({ type: 'PUSH_BUTTON' });
  console.log('at 1000:', JSON.stringify(actor.getSnapshot().value));
  clock.set(6000);
  console.log('at 6000:', JSON.stringify(actor.getSnapshot().value));
}
