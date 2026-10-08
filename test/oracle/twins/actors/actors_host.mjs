// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, fromPromise, setup } from 'xstate';

/** Prints each pending request: its src, its id and its input. */
function printRequests() {
  for (const request of requests) {
    console.log(`request ${request.src} ${request.id} ${JSON.stringify(request.input)}`);
  }
}

/** Prints an actor's state value and context. */
function print(actor) {
  const snapshot = actor.getSnapshot();
  console.log(`${JSON.stringify(snapshot.value)} ${JSON.stringify(snapshot.context)}`);
}

// The promises not settled yet: what host_requests() lists.
const requests = [];

// tag::machine[]
const userMachine = setup({
  actors: {
    fetchUser: fromPromise(
      ({ input, self }) =>
        new Promise((resolve, reject) => {
          requests.push({ src: self.src, id: self.id, input, resolve, reject });
        }),
    ),
  },
}).createMachine({
  id: 'user',
  initial: 'idle',
  context: { userId: '42', user: null, error: null },
  states: {
    idle: { on: { FETCH: { target: 'loading' } } },
    loading: {
      invoke: {
        id: 'getUser',
        src: 'fetchUser',
        input: ({ context: { userId } }) => ({ userId }),
        onDone: {
          target: 'success',
          actions: assign({ user: ({ event }) => event.output }),
        },
        onError: {
          target: 'failure',
          actions: assign({ error: ({ event }) => event.error }),
        },
      },
    },
    success: {},
    failure: { on: { RETRY: { target: 'loading' } } },
  },
});
// end::machine[]

// tag::run[]
const actor = createActor(userMachine);
actor.start();
actor.send({ type: 'FETCH' });
printRequests(); // request fetchUser getUser {"userId":"42"}
requests.shift().reject('Network error');
await new Promise((settled) => setTimeout(settled, 0));
print(actor); // "failure", with the error

actor.send({ type: 'RETRY' });
printRequests(); // a new request
requests.shift().resolve({ name: 'David', location: 'Florida' });
await new Promise((settled) => setTimeout(settled, 0));
print(actor); // "success", with the user
// end::run[]
