// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine } from 'xstate';

const feedbackMachine = createMachine({
  id: 'feedback',
  context: ({ input }) => ({
    userId: input.userId,
    feedback: '',
    rating: input.defaultRating,
  }),
  initial: 'prompt',
  states: {
    prompt: {},
  },
});

const feedbackActor = createActor(feedbackMachine, {
  input: {
    userId: '123',
    defaultRating: 5,
  },
});
feedbackActor.start();
console.log(JSON.stringify(feedbackActor.getSnapshot().context));
