// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { getInitialSnapshot, getMicrosteps, raise, setup } from 'xstate';

const loader = setup({
  actions: {
    announce: raise(({ context }) => ({
      type: 'started',
      attempt: context.attempt,
    })),
  },
}).createMachine({
  id: 'loader',
  initial: 'idle',
  context: { attempt: 1 },
  states: {
    idle: { on: { load: 'loading' } },
    loading: { entry: 'announce', on: { started: 'running' } },
    running: {},
  },
});

const idle = getInitialSnapshot(loader);
for (const [snapshot, actions] of getMicrosteps(loader, idle, {
  type: 'load',
})) {
  const returned = actions.map((a) => ` ${a.type} ${JSON.stringify(a.params)}`);
  console.log(JSON.stringify(snapshot.value) + returned.join(''));
}
