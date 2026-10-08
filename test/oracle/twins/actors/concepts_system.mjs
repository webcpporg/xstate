// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, sendTo, setup } from 'xstate';

const vetMachine = setup({
  actions: {
    note: assign(({ event }) => ({ last: event.symptom })),
  },
}).createMachine({
  id: 'vet',
  context: { last: null },
  on: { symptom: { actions: 'note' } },
});

const walkerMachine = setup({
  actions: {
    callVet: sendTo(
      ({ system }) => system.get('vet'),
      (_, params) => ({ type: 'symptom', symptom: params.symptom }),
    ),
  },
}).createMachine({
  id: 'walker',
  on: {
    'sees a limp': {
      actions: { type: 'callVet', params: { symptom: 'limps' } },
    },
  },
});

const ownerMachine = setup({
  actors: { vet: vetMachine, walker: walkerMachine },
}).createMachine({
  id: 'owner',
  invoke: [
    { id: 'vet', src: 'vet', systemId: 'vet' },
    { id: 'walker', src: 'walker' },
  ],
});

const ownerActor = createActor(ownerMachine).start();
ownerActor.getSnapshot().children.walker.send({ type: 'sees a limp' });
const vetActor = ownerActor.system.get('vet');
console.log(`vet: ${JSON.stringify(vetActor.getSnapshot().context)}`);
