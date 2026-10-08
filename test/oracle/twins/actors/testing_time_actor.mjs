// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import assert from 'node:assert/strict';
import { test } from 'node:test';
import { SimulatedClock, createActor, createMachine } from 'xstate';

test('should transition after delay', () => {
  const machine = createMachine({
    id: 'light',
    initial: 'green',
    states: {
      green: { after: { 1000: 'yellow' } },
      yellow: { after: { 1000: 'red' } },
      red: { after: { 1000: 'green' } },
    },
  });

  const clock = new SimulatedClock();
  const actor = createActor(machine, { clock });
  actor.start();

  const tick = (now, expected) => {
    clock.set(now);
    console.log(`after ${now} ms:`, JSON.stringify(actor.getSnapshot().value));
    assert.equal(actor.getSnapshot().value, expected);
  };

  tick(500, 'green');
  tick(1010, 'yellow');
});
