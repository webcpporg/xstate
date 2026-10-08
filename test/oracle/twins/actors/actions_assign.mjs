// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, createMachine } from 'xstate';

const countMachine = createMachine({
  context: { count: 0, unit: 'clicks' },
  on: {
    increment: {
      actions: assign({
        count: ({ context, event }) => context.count + event.value,
      }),
    },
  },
});

const countActor = createActor(countMachine);
countActor.subscribe((state) => {
  console.log(JSON.stringify(state.context));
});
countActor.start();
countActor.send({ type: 'increment', value: 3 });
countActor.send({ type: 'increment', value: 2 });
