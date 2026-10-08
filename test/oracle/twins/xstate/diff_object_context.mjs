// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createMachine, getInitialSnapshot, getNextSnapshot, setup } from 'xstate';

function describe(snapshot) {
  if (snapshot.status === 'error') {
    return `error, ${snapshot.error}`;
  }
  return JSON.stringify(snapshot.context);
}

const adder = setup({ actions: { add: assign({ x: 1 }) } });
for (const context of [null, false, 5, 'ab']) {
  const machine = adder.createMachine({
    context,
    on: { ADD: { actions: 'add' } },
  });
  const initial = getInitialSnapshot(machine);
  const added = getNextSnapshot(machine, initial, { type: 'ADD' });
  console.log(`context ${JSON.stringify(context)}: ${describe(initial)}, after ADD ${describe(added)}`);
}

const made = createMachine({ context: () => 'ab' });
console.log(`a context function that returns "ab": ${describe(getInitialSnapshot(made))}`);
