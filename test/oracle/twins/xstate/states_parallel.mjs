// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getMicrosteps, initialTransition, transition } from 'xstate';

const playerMachine = createMachine({
  id: 'player',
  type: 'parallel',
  states: {
    track: {
      initial: 'paused',
      states: {
        paused: { on: { PLAY: 'playing' } },
        playing: { on: { STOP: 'paused', RESET: 'paused' } },
      },
    },
    volume: {
      initial: 'normal',
      states: {
        normal: { on: { MUTE: 'muted' } },
        muted: { on: { UNMUTE: 'normal', RESET: 'normal' } },
      },
    },
  },
});

let [snapshot] = initialTransition(playerMachine);
console.log(JSON.stringify(snapshot.value));

[snapshot] = transition(playerMachine, snapshot, { type: 'PLAY' });
[snapshot] = transition(playerMachine, snapshot, { type: 'MUTE' });
console.log(JSON.stringify(snapshot.value));

const steps = getMicrosteps(playerMachine, snapshot, { type: 'RESET' });
console.log(`${steps.length} microstep: ${JSON.stringify(steps.at(-1)[0].value)}`);
