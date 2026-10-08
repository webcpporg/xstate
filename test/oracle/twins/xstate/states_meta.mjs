// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const feedbackMachine = createMachine({
  id: 'feedback',
  initial: 'prompt',
  meta: { title: 'Feedback' },
  states: {
    prompt: {
      description: 'Waits for the user to rate the experience.',
      meta: { content: 'How was your experience?' },
      on: { 'feedback.good': 'thanks' },
    },
    thanks: { meta: { content: 'Thank you for your feedback!' } },
  },
});

let [snapshot] = initialTransition(feedbackMachine);
console.log(JSON.stringify(snapshot.getMeta()));

[snapshot] = transition(feedbackMachine, snapshot, { type: 'feedback.good' });
console.log(JSON.stringify(snapshot.getMeta()));

console.log(feedbackMachine.getStateNodeById('feedback.prompt').description);
