// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, fromPromise, setup, spawnChild } from 'xstate';

const feedbackMachine = setup({
  actors: {
    emailUser: fromPromise(async ({ input }) => {
      console.log(JSON.stringify(input));
    }),
  },
}).createMachine({
  id: 'feedback',
  context: { userId: '123' },
  on: {
    'feedback.submit': {
      actions: spawnChild('emailUser', {
        id: 'emailUser',
        input: ({ context }) => ({ userId: context.userId }),
      }),
    },
  },
});

const feedbackActor = createActor(feedbackMachine).start();
feedbackActor.send({ type: 'feedback.submit' });
