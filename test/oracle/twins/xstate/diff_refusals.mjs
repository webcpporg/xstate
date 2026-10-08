// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine, getInitialSnapshot, getNextSnapshot } from 'xstate';

// Prints that createMachine accepts `config`, then what `call` makes of the
// machine, or the first line of what it throws.
function report(label, config, name, call) {
  const machine = createMachine(config);
  console.log(`${label}: created`);
  try {
    console.log(`  ${name}: ${call(machine)}`);
  } catch (error) {
    console.log(`  ${name} throws: ${error.message.split('\n')[0]}`);
  }
}

const noSuchInitial = {
  id: 'm',
  initial: 'nowhere',
  states: { a: {} },
};
const unknownGuard = {
  id: 'm',
  initial: 'a',
  states: {
    a: { on: { GO: { target: 'b', guard: 'isReady' } } },
    b: {},
  },
};
const unknownDelay = {
  id: 'm',
  initial: 'a',
  states: {
    a: { after: { slow: 'b' } },
    b: {},
  },
};
const endlessEntry = {
  id: 'm',
  initial: 'a',
  states: {
    a: {
      initial: 'h',
      states: {
        h: { type: 'history', target: '#m.a' },
        x: {},
      },
    },
  },
};
const nullExit = {
  id: 'm',
  initial: 'a',
  states: {
    a: { exit: null, on: { GO: 'b' } },
    b: {},
  },
};
const unknownActor = {
  id: 'm',
  invoke: { id: 'w', src: 'worker' },
};

report('an initial that names no child', noSuchInitial, 'getInitialSnapshot', (machine) =>
  JSON.stringify(getInitialSnapshot(machine).value),
);
report('a guard with no implementation', unknownGuard, 'GO', (machine) =>
  JSON.stringify(getNextSnapshot(machine, getInitialSnapshot(machine), { type: 'GO' }).value),
);
report('a delay with no implementation', unknownDelay, 'the initial snapshot', (machine) =>
  JSON.stringify(getInitialSnapshot(machine).value),
);
report('a default entry that leads back', endlessEntry, 'the initial snapshot', (machine) => {
  const initial = getInitialSnapshot(machine);
  return `${initial.status}, ${initial.error}`;
});
report('an exit that is null', nullExit, 'GO', (machine) =>
  JSON.stringify(getNextSnapshot(machine, getInitialSnapshot(machine), { type: 'GO' }).value),
);
report('an invoke of an unknown actor', unknownActor, 'start()', (machine) => {
  const actor = createActor(machine);
  actor.subscribe({ error: () => {} });
  actor.start();
  return `${actor.getSnapshot().status}, ${actor.getSnapshot().error}`;
});
