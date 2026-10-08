// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const playerMachine = createMachine({
  id: 'player',
  initial: 'powered',
  states: {
    powered: {
      initial: 'radio',
      states: {
        radio: {
          initial: 'stopped',
          states: {
            stopped: { on: { play: 'playing' } },
            playing: { on: { stop: 'stopped' } },
          },
        },
        shallow: { type: 'history' },
        deep: { type: 'history', history: 'deep' },
      },
      on: { power: 'standby' },
    },
    standby: {
      on: {
        resume: 'powered.shallow',
        resumeDeep: 'powered.deep',
      },
    },
  },
});

let [standby] = initialTransition(playerMachine);
for (const type of ['play', 'power']) {
  [standby] = transition(playerMachine, standby, { type });
}
const remembered = (id) =>
  JSON.stringify(standby.historyValue[id].map((node) => node.id));
console.log(`shallow remembers ${remembered('player.powered.shallow')}`);
console.log(`deep remembers ${remembered('player.powered.deep')}`);

for (const type of ['resume', 'resumeDeep']) {
  const [resumed] = transition(playerMachine, standby, { type });
  console.log(`${type}: ${JSON.stringify(resumed.value)}`);
}
