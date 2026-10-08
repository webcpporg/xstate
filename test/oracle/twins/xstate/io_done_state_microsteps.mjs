// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, setup } from 'xstate';

const coffeeMachine = setup({
  actions: { keepDose: assign({ dose: ({ event }) => event.output }) },
}).createMachine({
  id: 'coffee',
  context: { dose: null },
  initial: 'preparation',
  states: {
    preparation: {
      initial: 'weighing',
      states: {
        weighing: { on: { weighed: 'grinding' } },
        grinding: { on: { ground: 'ready' } },
        ready: {
          type: 'final',
          output: ({ event }) => ({ grams: event.grams }),
        },
      },
      onDone: { target: 'brewing', actions: 'keepDose' },
    },
    brewing: {},
  },
});

const coffeeActor = createActor(coffeeMachine).start();
coffeeActor.send({ type: 'weighed' });

coffeeActor.system.inspect((inspectionEvent) => {
  if (inspectionEvent.type === '@xstate.microstep') {
    console.log(
      JSON.stringify(inspectionEvent.event),
      '->',
      JSON.stringify(inspectionEvent.snapshot.value),
    );
  }
});
coffeeActor.send({ type: 'ground', grams: 18 });
