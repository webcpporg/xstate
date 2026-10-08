// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, createMachine } from 'xstate';

const countMachine = createMachine({
  context: {
    count: 0,
  },
  on: {
    INC: {
      actions: assign({
        count: ({ context }) => context.count + 1,
      }),
    },
    DEC: {
      actions: assign({
        count: ({ context }) => context.count - 1,
      }),
    },
    SET: {
      actions: assign({
        count: ({ event }) => event.value,
      }),
    },
  },
});

const countActor = createActor(countMachine).start();

countActor.subscribe((state) => {
  console.log(state.context.count);
});

countActor.send({ type: 'INC' });
// logs 1
countActor.send({ type: 'DEC' });
// logs 0
countActor.send({ type: 'SET', value: 10 });
// logs 10
