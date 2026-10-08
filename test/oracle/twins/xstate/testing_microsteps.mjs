// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { createMachine, getMicrosteps, initialTransition } from 'xstate';

test('every microstep', () => {
  const machine = createMachine({
    initial: 'idle',
    states: {
      idle: { on: { submit: 'validating' } },
      validating: { entry: 'validate', always: 'submitted' },
      submitted: {},
    },
  });

  const [initial] = initialTransition(machine);
  const microsteps = getMicrosteps(machine, initial, { type: 'submit' });
  microsteps.forEach(([snapshot, actions], index) => {
    const types = `[${actions.map((action) => action.type).join(', ')}]`;
    const settled = index === microsteps.length - 1 ? ' settled' : '';
    console.log(`${JSON.stringify(snapshot.value)} ${types}${settled}`);
  });

  // The transient state was entered, then left in the same macrostep.
  const values = microsteps.map(([snapshot]) => snapshot.value);
  assert.deepEqual(values, ['validating', 'submitted']);
});
