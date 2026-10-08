// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { assign, createActor, setup } from 'xstate';

test('a failing assign', () => {
  // 1. Arrange
  const machine = setup({
    actions: {
      saveAge: assign(({ event }) => {
        if (typeof event.age !== 'number') {
          throw new Error('no age');
        }
        return { age: event.age };
      }),
    },
  }).createMachine({
    initial: 'editing',
    context: { age: 0 },
    states: {
      editing: { on: { save: { target: 'saved', actions: 'saveAge' } } },
      saved: {},
    },
  });
  const actor = createActor(machine);
  const errors = [];
  actor.subscribe({ error: (error) => errors.push(error.message) });

  // 2. Act
  actor.start();
  actor.send({ type: 'save' });

  // 3. Assert
  const failed = actor.getSnapshot();
  assert.equal(failed.status, 'error');
  assert.deepEqual(errors, ['no age']);
  assert.equal(failed.value, 'editing');
  console.log('status:', failed.status);
  console.log('value:', JSON.stringify(failed.value));
  console.log('context:', JSON.stringify(failed.context));
});
