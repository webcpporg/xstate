// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { createMachine } from 'xstate';
import { getShortestPaths } from 'xstate/graph';

test('a shortest path to every state', () => {
  const machine = createMachine({
    initial: 'idle',
    states: {
      idle: { on: { start: 'running' } },
      running: { on: { pause: 'paused', stop: 'idle' } },
      paused: { on: { resume: 'running', stop: 'idle' } },
    },
  });

  const paths = getShortestPaths(machine);
  assert.equal(paths.length, 3);
  for (const path of paths) {
    const events = path.steps.map((step) => step.event.type);
    console.log(`${JSON.stringify(path.state.value)}: [${events.join(', ')}]`);
  }
});
