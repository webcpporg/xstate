// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const processMachine = createMachine({
  id: 'process',
  initial: 'running',
  states: {
    running: { on: { finish: 'complete' } },
    complete: { type: 'final' },
  },
  output: { message: 'Process completed.' },
});

let [snapshot] = initialTransition(processMachine);
console.log(JSON.stringify(snapshot.value), snapshot.status);

[snapshot] = transition(processMachine, snapshot, { type: 'finish' });
console.log(JSON.stringify(snapshot.value), snapshot.status);
if (snapshot.status === 'done') {
  console.log('output:', JSON.stringify(snapshot.output));
}
