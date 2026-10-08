// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { createActor, fromPromise, setup } from 'xstate';

test('mocking promise actors', async () => {
  const calls = [];
  const mockFetch = async () => {
    calls.push('fetchData');
    return { data: 'test' };
  };

  const machine = setup({
    actors: {
      fetchData: fromPromise(mockFetch),
    },
  }).createMachine({
    initial: 'idle',
    states: {
      idle: {
        on: {
          fetch: 'loading',
        },
      },
      loading: {
        invoke: {
          src: 'fetchData',
          onDone: 'success',
          onError: 'error',
        },
      },
      success: {},
      error: {},
    },
  });

  const actor = createActor(machine);
  actor.start();

  actor.send({ type: 'fetch' });
  console.log('requests:', calls.join(', '));

  // Wait for promise to resolve
  await new Promise((resolve) => setTimeout(resolve, 0));

  assert.equal(actor.getSnapshot().value, 'success');
  assert.deepEqual(calls, ['fetchData']);
  console.log('value:', JSON.stringify(actor.getSnapshot().value));
});
