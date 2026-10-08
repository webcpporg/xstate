// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine } from 'xstate';

const toggleMachine = createMachine({
  initial: 'inactive',
  states: {
    inactive: { on: { toggle: { target: 'active' } } },
    active: { on: { toggle: { target: 'inactive' } } },
  },
});

const toggleActor = createActor(toggleMachine);
toggleActor.subscribe((snapshot) => {
  console.log(JSON.stringify(snapshot.value));
});

toggleActor.start();
// logs "inactive"
toggleActor.send({ type: 'toggle' });
// logs "active"
toggleActor.send({ type: 'toggle' });
// logs "inactive"

toggleActor.stop();
console.log('status: ' + toggleActor.getSnapshot().status);
