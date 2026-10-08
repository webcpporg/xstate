// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, getMicrosteps, initialTransition, not, setup } from 'xstate';

const kettleMachine = setup({
  guards: {
    isBoiling: ({ context }) => context.temperature > 100
  },
  actions: {
    updateTemperature: assign({ temperature: ({ event }) => event.temperature })
  }
}).createMachine({
  id: 'kettle',
  initial: 'lukewarm',
  context: { temperature: 80 },
  on: { 'temp.update': { actions: 'updateTemperature' } },
  states: {
    lukewarm: { on: { boil: 'heating' } },
    heating: { always: { guard: 'isBoiling', target: 'boiling' } },
    boiling: {
      entry: 'turnOffLight',
      always: { guard: not('isBoiling'), target: 'heating' }
    }
  }
});

const typesOf = (actions) => `[${actions.map((action) => action.type).join(', ')}]`;

let [now] = initialTransition(kettleMachine);
for (const event of [
  { type: 'temp.update', temperature: 105 },
  { type: 'boil' },
  { type: 'temp.update', temperature: 90 }
]) {
  let line = `${event.type}:`;
  for (const [snapshot, actions] of getMicrosteps(kettleMachine, now, event)) {
    line += ` ${JSON.stringify(snapshot.value)} ${typesOf(actions)}`;
    now = snapshot;
  }
  console.log(line);
}
