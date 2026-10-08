// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, fromPromise, setup } from 'xstate';

const feedbackMachine = setup({
  actors: {
    liveFeedback: fromPromise(async ({ input }) => {
      console.log(JSON.stringify(input));
      return {};
    }),
  },
}).createMachine({
  id: 'feedback',
  invoke: {
    src: 'liveFeedback',
    input: {
      domain: 'stately.ai',
    },
  },
});

createActor(feedbackMachine).start();
