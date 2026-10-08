// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { createActor, setup } from 'xstate';

test('some actor', () => {
  const notifiedMessages = [];

  // 1. Arrange
  const machine = setup({
    actions: {
      notify: (_, params) => {
        notifiedMessages.push(params.message);
      },
    },
  }).createMachine({
    initial: 'inactive',
    states: {
      inactive: {
        on: { toggle: { target: 'active' } },
      },
      active: {
        entry: { type: 'notify', params: { message: 'Active!' } },
        on: { toggle: { target: 'inactive' } },
      },
    },
  });
  const actor = createActor(machine);

  // 2. Act
  actor.start();
  actor.send({ type: 'toggle' }); // => 'active'
  actor.send({ type: 'toggle' }); // => 'inactive'
  actor.send({ type: 'toggle' }); // => 'active'

  // 3. Assert
  assert.equal(actor.getSnapshot().value, 'active');
  assert.deepEqual(notifiedMessages, ['Active!', 'Active!']);
  console.log('value:', JSON.stringify(actor.getSnapshot().value));
  console.log('notified:', JSON.stringify(notifiedMessages));
});
