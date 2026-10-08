// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const config = {
  id: 'checkout',
  initial: 'paying',
  states: {
    paying: { on: { paid: 'receipt' } },
    receipt: {
      type: 'final',
      output: ({ event }) => ({ receiptId: event.receiptId }),
    },
  },
};
const withOutput = createMachine({ ...config, output: ({ event }) => event });
const withoutOutput = createMachine(config);

function checkoutOutput(checkout) {
  const [start] = initialTransition(checkout);
  const [done] = transition(checkout, start, { type: 'paid', receiptId: 'R-1' });
  if (done.status !== 'done') {
    return 'error';
  }
  return done.output === undefined ? 'none' : JSON.stringify(done.output);
}

console.log(checkoutOutput(withOutput));
console.log(checkoutOutput(withoutOutput));
