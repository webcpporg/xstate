// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getInitialMicrosteps, initialTransition } from 'xstate';

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
const input = { userId: '123', defaultRating: 5 };

const [started] = initialTransition(feedbackMachine, input);
console.log(JSON.stringify(started.context));

const microsteps = getInitialMicrosteps(feedbackMachine, input);
const [settled] = microsteps[microsteps.length - 1];
console.log(`${JSON.stringify(settled.context)} after ${microsteps.length} microstep`);
