// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, emit, setup } from 'xstate';

const machine = setup({
  actions: { announce: emit({ type: 'activated' }) },
}).createMachine({
  initial: 'inactive',
  states: {
    inactive: { on: { toggle: 'active' } },
    active: { entry: 'announce', on: { toggle: 'inactive' } },
  },
});

const actor = createActor(machine, {
  inspect: (inspection) => {
    if (inspection.type === '@xstate.actor') {
      console.log('started');
    }
    if (inspection.type === '@xstate.snapshot') {
      console.log(
        'settled ' +
          inspection.event.type +
          ' ' +
          JSON.stringify(inspection.snapshot.value),
      );
    }
  },
});
actor.on('*', (emitted) => {
  console.log('emitted ' + emitted.type);
});
actor.start();
actor.send({ type: 'toggle' });
