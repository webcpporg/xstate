// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const checkoutMachine = createMachine({
  id: 'checkout',
  initial: 'cart',
  states: {
    cart: { on: { checkout: 'payment.hist' } },
    payment: {
      initial: 'card',
      states: {
        card: { on: { switch: 'paypal' } },
        paypal: { on: { switch: 'card' } },
        hist: { type: 'history' },
      },
      on: { next: 'address' },
    },
    address: { on: { back: 'payment.hist' } },
  },
});

let [snapshot] = initialTransition(checkoutMachine);
console.log(JSON.stringify(snapshot.value));

for (const type of ['checkout', 'switch', 'next']) {
  [snapshot] = transition(checkoutMachine, snapshot, { type });
  console.log(`${type}: ${JSON.stringify(snapshot.value)}`);
}

const remembered = snapshot.historyValue['checkout.payment.hist'];
console.log(`remembered: ${JSON.stringify(remembered.map((node) => node.id))}`);

[snapshot] = transition(checkoutMachine, snapshot, { type: 'back' });
console.log(`back: ${JSON.stringify(snapshot.value)}`);
