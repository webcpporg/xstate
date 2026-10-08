// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { createMachine, getMicrosteps, initialTransition } from 'xstate';

test('a bounded macrostep', () => {
  const machine = createMachine({
    initial: 'idle',
    options: { maxIterations: 1000 },
    states: {
      idle: { on: { GO: 'one', SPIN: 'ping' } },
      one: { entry: 'first', always: 'two' },
      two: { entry: 'second', always: 'three' },
      three: { entry: 'third' },
      ping: { always: 'pong' },
      pong: { always: 'ping' },
    },
  });
  const [idle] = initialTransition(machine);

  const went = getMicrosteps(machine, idle, { type: 'GO' });
  const [settled] = went[went.length - 1];
  const actions = went.flatMap(([, returned]) => returned.map((action) => action.type));
  assert.equal(settled.value, 'three');
  console.log(
    `GO settles in ${went.length} microsteps at ${JSON.stringify(settled.value)}` +
      ` with [${actions.join(', ')}]`,
  );

  assert.throws(() => getMicrosteps(machine, idle, { type: 'SPIN' }), /Infinite loop detected/);
  console.log('SPIN has not settled after 1000 microsteps');
});
