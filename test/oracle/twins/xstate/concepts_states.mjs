// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition } from 'xstate';

const dogMachine = createMachine({
  id: 'dog',
  initial: 'asleep',
  states: {
    asleep: {},
    awake: {},
  },
});

const [first] = initialTransition(dogMachine);
console.log(`value: ${JSON.stringify(first.value)}`);
console.log(`asleep: ${first.matches('asleep')}`);
console.log(`awake: ${first.matches('awake')}`);
