// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getInitialSnapshot, getNextSnapshot } from 'xstate';

const light = createMachine({
  id: 'light',
  initial: 'green',
  states: {
    green: { on: { TIMER: 'yellow' } },
    yellow: { on: { TIMER: 'red' } },
    red: { on: { TIMER: 'green' } },
  },
});

const green = getInitialSnapshot(light);
const yellow = getNextSnapshot(light, green, { type: 'TIMER' });
const red = getNextSnapshot(light, yellow, { type: 'TIMER' });
const stillRed = getNextSnapshot(light, red, { type: 'UNKNOWN' });

console.log(JSON.stringify(green.value));
console.log(JSON.stringify(yellow.value));
console.log(JSON.stringify(red.value));
console.log(JSON.stringify(stillRed.value));
