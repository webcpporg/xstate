// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine } from 'xstate';

const player = createMachine({
  id: 'player',
  initial: 'stopped',
  states: {
    stopped: { on: { play: 'playing' } },
    playing: {
      id: 'active',
      initial: 'normal',
      states: { normal: {}, fast: {} },
      on: { stop: 'stopped' },
    },
    done: { type: 'final' },
  },
});

const visit = (node) => {
  console.log(`${node.order} ${node.key} ${node.id} ${node.type}`);
  Object.values(node.states).forEach(visit);
};
visit(player.root);

const active = player.getStateNodeById('active');
const fast = active.states.fast;
const found = player.getStateNodeById('#active.fast');
console.log(`${fast.id} ${found.id}`);
