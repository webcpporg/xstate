// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { assign, createActor, setup } from 'xstate';

test('actor transitions correctly', () => {
  const toggleMachine = setup({}).createMachine({
    initial: 'inactive',
    context: { count: 0 },
    states: {
      inactive: {
        on: {
          activate: {
            target: 'active',
            actions: assign({ count: ({ context }) => context.count + 1 }),
          },
        },
      },
      active: {
        on: { deactivate: 'inactive' },
      },
    },
  });

  const actor = createActor(toggleMachine);
  actor.start();

  const expectSnapshot = (value, context) => {
    const now = actor.getSnapshot();
    console.log(JSON.stringify(now.value), JSON.stringify(now.context));
    assert.deepEqual(now.value, value);
    assert.deepEqual(now.context, context);
  };

  // Test initial state
  expectSnapshot('inactive', { count: 0 });

  // Send event and test transition
  actor.send({ type: 'activate' });
  expectSnapshot('active', { count: 1 });

  // Send another event
  actor.send({ type: 'deactivate' });
  expectSnapshot('inactive', { count: 1 });
});
