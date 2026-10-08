// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getInitialSnapshot } from 'xstate';

/** Prints where XState refuses a config: when it is created, or when it starts. */
function tryConfig(config) {
  let machine;
  try {
    machine = createMachine(config);
  } catch (error) {
    console.log('createMachine throws: ' + error.message);
    return;
  }
  try {
    getInitialSnapshot(machine);
    console.log('created and started');
  } catch (error) {
    console.log('created; getInitialSnapshot throws: ' + error.message);
  }
}

tryConfig({
  initial: 'red',
  states: {
    red: { states: { walk: {}, wait: {} } },
  },
});
tryConfig({
  initial: 'nowhere',
  states: { red: {} },
});
