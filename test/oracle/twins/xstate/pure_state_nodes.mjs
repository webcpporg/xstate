// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getInitialSnapshot, getNextSnapshot } from 'xstate';

// XState keeps a snapshot's active state nodes in _nodes, which is not part
// of its public API; the oracle reads the same field (serialize.mjs).
const printNodes = (snapshot) => {
  console.log(JSON.stringify(snapshot.value));
  console.log(snapshot._nodes.map((node) => ` ${node.id}`).join(''));
};

const player = createMachine({
  id: 'player',
  initial: 'on',
  states: {
    off: {},
    on: {
      type: 'parallel',
      on: { OFF: '#player.off' },
      states: {
        track: {
          initial: 'paused',
          states: {
            paused: { on: { PLAY: 'playing' } },
            playing: {},
          },
        },
        volume: {
          initial: 'normal',
          states: {
            normal: { on: { MUTE: 'muted' } },
            muted: {},
          },
        },
      },
    },
  },
});

const paused = getInitialSnapshot(player);
printNodes(paused);
const playing = getNextSnapshot(player, paused, { type: 'PLAY' });
printNodes(playing);
