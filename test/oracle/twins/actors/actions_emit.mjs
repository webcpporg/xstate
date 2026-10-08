// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, emit, setup } from 'xstate';

const machine = setup({
  actions: {
    emitStaticEvent: emit({ type: 'someStaticEvent', data: 42 }),
    emitDynamicEvent: emit(({ context }) => ({
      type: 'someDynamicEvent',
      data: context.someData,
    })),
    track: () => console.log('action: track'),
  },
}).createMachine({
  context: { someData: 'hello' },
  on: {
    someEvent: {
      actions: ['emitStaticEvent', 'emitDynamicEvent', 'track'],
    },
  },
});

const actor = createActor(machine);
actor.on('someStaticEvent', (emitted) => {
  console.log('static:', JSON.stringify(emitted));
});
actor.on('*', (emitted) => {
  console.log('any:', emitted.type);
});

actor.start();
actor.send({ type: 'someEvent' });
