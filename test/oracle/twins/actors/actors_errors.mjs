// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, fromPromise, log, setup } from 'xstate';

const requests = [];

const checkout = setup({
  actors: {
    charge: fromPromise(
      () =>
        new Promise((resolve, reject) => {
          requests.push({ resolve, reject });
        }),
    ),
  },
}).createMachine({
  initial: 'paying',
  states: {
    paying: { invoke: { src: 'charge', id: 'payment' } },
  },
});

const shopMachine = setup({
  actors: { checkout },
  actions: { note: log(({ event }) => event, 'shop') },
}).createMachine({
  initial: 'open',
  states: {
    open: {
      invoke: {
        src: 'checkout',
        id: 'checkout',
        onError: { target: 'failed', actions: 'note' },
      },
    },
    failed: {},
  },
});

const shop = createActor(shopMachine, {
  logger: (label, value) => console.log(label + ' ' + JSON.stringify(value)),
});
shop.start();
const checkoutActor = shop.getSnapshot().children.checkout;
requests.shift().reject({ code: 'declined' });
await new Promise((done) => setTimeout(done, 0));
// logs the error event onError took

const failed = checkoutActor.getSnapshot();
console.log('checkout: ' + failed.status + ' ' + JSON.stringify(failed.error));
