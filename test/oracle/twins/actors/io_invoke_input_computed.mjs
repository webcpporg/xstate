// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, fromPromise, setup } from 'xstate';

const feedbackMachine = setup({
  actors: {
    fetchUser: fromPromise(async ({ input }) => {
      console.log(JSON.stringify(input));
      return {};
    }),
  },
  actions: {
    selectUser: assign({ userId: ({ event }) => event.userId }),
  },
}).createMachine({
  id: 'feedback',
  context: { userId: '' },
  initial: 'idle',
  states: {
    idle: {
      on: { 'user.selected': 'loading' },
    },
    loading: {
      entry: 'selectUser',
      invoke: {
        id: 'fetchUser',
        src: 'fetchUser',
        input: ({ context }) => ({ userId: context.userId }),
      },
    },
  },
});

const feedbackActor = createActor(feedbackMachine).start();
feedbackActor.send({ type: 'user.selected', userId: '42' });
