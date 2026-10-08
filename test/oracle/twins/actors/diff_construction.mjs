// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine, setup } from 'xstate';

const worker = createMachine({});
const app = setup({ actors: { worker } }).createMachine({
  initial: 'running',
  states: {
    running: { invoke: { id: 'w', src: 'worker', systemId: 'worker' } },
  },
});

function describe(moment, actor) {
  const registered = actor.system.get('worker') !== undefined ? 'registered' : 'unknown';
  const value = JSON.stringify(actor.getSnapshot().value);
  console.log(`${moment}: ${value}, worker ${registered}`);
}

const actor = createActor(app);
describe('before start', actor);
actor.start();
describe('after start', actor);
