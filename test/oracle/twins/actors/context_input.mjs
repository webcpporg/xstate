// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, createMachine, getInitialSnapshot } from 'xstate';

const feedbackMachine = createMachine({
  id: 'feedback',
  context: ({ input }) => ({
    feedback: '',
    rating: input.defaultRating,
  }),
});

const input = { defaultRating: 5 };
const snapshot = getInitialSnapshot(feedbackMachine, input);
console.log(JSON.stringify(snapshot.context));

const without = getInitialSnapshot(feedbackMachine);
console.log(without.status, JSON.stringify(without.context));

const feedbackActor = createActor(feedbackMachine, { input }).start();
console.log(JSON.stringify(feedbackActor.getSnapshot().context));
