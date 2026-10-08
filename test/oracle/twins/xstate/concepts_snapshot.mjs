// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const walkMachine = createMachine({
  id: 'walk',
  initial: 'waiting',
  context: { dog: 'Rex' },
  states: {
    waiting: { on: { 'leave home': 'onAWalk' } },
    onAWalk: {
      tags: ['outside'],
      initial: 'walking',
      states: {
        walking: { on: { 'speed up': 'running' } },
        running: {},
      },
      on: { 'arrive home': 'waiting' },
    },
  },
});

const [waiting] = initialTransition(walkMachine);
const [out] = transition(walkMachine, waiting, { type: 'leave home' });

console.log(`value: ${JSON.stringify(out.value)}`);
console.log(`context: ${JSON.stringify(out.context)}`);
console.log(`status: ${out.status}`);
console.log(`tags: ${JSON.stringify([...out.tags])}`);
console.log(`matches onAWalk: ${out.matches('onAWalk')}`);
console.log(`has tag outside: ${out.hasTag('outside')}`);
console.log(`can arrive home: ${out.can({ type: 'arrive home' })}`);
console.log(`can leave home: ${out.can({ type: 'leave home' })}`);
