// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, createMachine, setup } from 'xstate';

const quoteMachine = createMachine({
  id: 'quote',
  context: ({ input }) => input,
  initial: 'priced',
  states: {
    priced: { type: 'final' },
  },
  output: ({ context }) => ({ total: context.items * context.unitPrice }),
});

const orderMachine = setup({
  actors: { quote: quoteMachine },
}).createMachine({
  id: 'order',
  context: { quote: null },
  initial: 'quoting',
  states: {
    quoting: {
      invoke: {
        id: 'quote',
        src: 'quote',
        input: { items: 3, unitPrice: 4 },
        onDone: {
          target: 'quoted',
          actions: [
            ({ event }) => console.log(JSON.stringify(event)),
            assign({ quote: ({ event }) => event.output }),
          ],
        },
      },
    },
    quoted: {},
  },
});

const order = createActor(orderMachine).start();
const quoted = order.getSnapshot();
console.log(JSON.stringify(quoted.value), JSON.stringify(quoted.context));
