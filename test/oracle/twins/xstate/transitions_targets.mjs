// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const machine = createMachine({
  id: 'machine',
  initial: 'a',
  on: { reset: '.a' },
  states: {
    a: { on: { next: 'b', deep: 'b.two' } },
    b: {
      initial: 'one',
      on: { next: '#c' },
      states: { one: {}, two: {} }
    },
    c: {
      id: 'c',
      initial: 'child',
      on: { next: '.other' },
      states: { child: {}, other: {} }
    }
  }
});

let [now] = initialTransition(machine);
console.log('start', JSON.stringify(now.value));
for (const type of ['next', 'next', 'next', 'reset', 'deep']) {
  [now] = transition(machine, now, { type });
  console.log(type, '->', JSON.stringify(now.value));
}
