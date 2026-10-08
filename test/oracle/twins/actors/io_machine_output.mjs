// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, setup } from 'xstate';

const currencyMachine = setup({
  actions: {
    keepAmount: assign({ amount: ({ event }) => event.amount }),
  },
}).createMachine({
  id: 'currency',
  context: ({ input }) => ({
    amount: input.amount,
    currency: input.toCurrency,
  }),
  initial: 'converting',
  states: {
    converting: {
      on: {
        'amount.converted': { target: 'converted', actions: 'keepAmount' },
      },
    },
    converted: { type: 'final' },
  },
  output: ({ context }) => ({
    amount: context.amount,
    currency: context.currency,
  }),
});

const currencyActor = createActor(currencyMachine, {
  input: {
    amount: 10,
    fromCurrency: 'USD',
    toCurrency: 'EUR',
  },
});
currencyActor.subscribe((snapshot) => {
  if (snapshot.status === 'done') {
    console.log(JSON.stringify(snapshot.output));
  }
});
currencyActor.start();
currencyActor.send({ type: 'amount.converted', amount: 12 });
