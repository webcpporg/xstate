// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { createActor, setup } from 'xstate';

test('mocking actions', () => {
  const calls = [];
  const machine = setup({
    actions: {
      // Mock the logging action: record each call
      logMessage: (_, params) => {
        calls.push(`logMessage ${JSON.stringify(params)}`);
      },
    },
  }).createMachine({
    initial: 'idle',
    states: {
      idle: {
        on: {
          start: {
            target: 'running',
            actions: {
              type: 'logMessage',
              params: { message: 'Started!' },
            },
          },
        },
      },
      running: {},
    },
  });

  const actor = createActor(machine);
  actor.start();

  actor.send({ type: 'start' });

  assert.equal(actor.getSnapshot().value, 'running');
  assert.deepEqual(calls, ['logMessage {"message":"Started!"}']);
  console.log('value:', JSON.stringify(actor.getSnapshot().value));
  calls.forEach((call) => console.log(call));
});
