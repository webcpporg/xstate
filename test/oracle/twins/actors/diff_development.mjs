// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine, forwardTo, getInitialSnapshot, getNextSnapshot } from 'xstate';

try {
  createMachine({
    id: 'm',
    initial: 'a',
    states: {
      a: { on: { GO: { target: 'b', cond: 'ready' } } },
      b: {},
    },
  });
  console.log('a transition that declares cond: created');
} catch (error) {
  console.log(`a transition that declares cond: throws: ${error.message}`);
}

const anything = createMachine({
  id: 'm',
  on: { '*': { actions: 'heard' } },
});
try {
  const start = getInitialSnapshot(anything);
  const afterStar = getNextSnapshot(anything, start, { type: '*' });
  console.log(`an event of the type * from outside: ${afterStar.status}`);
} catch (error) {
  console.log(`an event of the type * from outside: throws: ${error.message}`);
}

const relay = createMachine({
  id: 'relay',
  on: {
    PING: { actions: forwardTo(({ system }) => system.get('nobody')) },
  },
});
const actor = createActor(relay);
actor.subscribe({ error: () => {} });
actor.start();
actor.send({ type: 'PING' });
const forwarded = actor.getSnapshot();
console.log(`a forwardTo to no actor: ${forwarded.status}, ${forwarded.error?.message}`);

// The development build warns about these descriptors on the standard
// error; each warning is printed here, under the event that made it.
const warnings = [];
console.warn = (message) => warnings.push(message);
const pointer = createMachine({
  id: 'pointer',
  initial: 'idle',
  states: {
    idle: { on: { 'mouse.*.*': 'deep', 'mou*se.*': 'starred' } },
    deep: {},
    starred: {},
  },
});
const idle = getInitialSnapshot(pointer);
for (const type of ['mouse.x', 'mouse.x', 'mou*se.x', 'mouse.*.*']) {
  const next = getNextSnapshot(pointer, idle, { type });
  console.log(`the event ${type}: ${JSON.stringify(next.value)}`);
  for (const warning of warnings.splice(0)) {
    console.log(`  warns: ${warning}`);
  }
}
