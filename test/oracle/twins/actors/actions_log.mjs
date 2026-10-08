// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, getInitialSnapshot, log, setup, transition } from 'xstate';

const counter = setup({
  actions: {
    add: assign({ count: ({ context }) => context.count + 1 }),
    logMessage: log('incremented'),
    logCount: log(({ context }) => context.count, 'count'),
    logAll: log(),
  },
}).createMachine({
  context: { count: 0 },
  on: {
    increment: { actions: ['add', 'logMessage', 'logCount', 'logAll'] },
  },
});

const increment = { type: 'increment' };

const initial = getInitialSnapshot(counter);
const [next, actions] = transition(counter, initial, increment);
for (const action of actions) {
  console.log(`${action.type} ${JSON.stringify(action.params)}`);
}

const actor = createActor(counter, {
  logger: (...args) => {
    const value = args.at(-1);
    const label = args.length === 2 ? `${args[0]}: ` : '';
    console.log(label + (value === undefined ? 'undefined' : JSON.stringify(value)));
  },
});
actor.start();
actor.send(increment);
