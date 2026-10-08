// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, fromPromise, setup } from 'xstate';

const config = {
  id: 'app',
  initial: 'loading',
  context: { user: null },
  states: {
    loading: {
      invoke: {
        id: 'fetch',
        src: 'fetchUser',
        systemId: 'fetcher',
        input: { userId: 42 },
        onDone: { target: 'ready', actions: 'keepUser' }
      }
    },
    ready: {}
  }
};

const requests = [];
const app = setup({
  actors: {
    fetchUser: fromPromise(
      ({ input, self }) => new Promise((resolve) => requests.push({ self, input, resolve }))
    )
  },
  actions: {
    keepUser: assign({ user: ({ event }) => event.output })
  }
}).createMachine(config);

function print(actor) {
  const snapshot = actor.getSnapshot();
  const value = JSON.stringify(snapshot.value);
  const context = JSON.stringify(snapshot.context);
  const children = Object.keys(snapshot.children).join(', ');
  console.log(`app: ${value} ${context} children [${children}]`);
}

const actor = createActor(app);
actor.start();
print(actor);
for (const request of requests) {
  console.log(`request: ${request.self.id} ${request.self.src} ${JSON.stringify(request.input)}`);
  console.log(`fetcher is the request's actor: ${actor.system.get('fetcher') === request.self}`);
}
requests[0].resolve({ name: 'Ana' });
await new Promise((settle) => setTimeout(settle, 0));
print(actor);
console.log(`fetcher is registered: ${actor.system.get('fetcher') !== undefined}`);
