// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, getInitialSnapshot, setup, transition } from 'xstate';

const feedbackMachine = setup({
  actions: {
    // tag::assigner[]
    updateFeedback: assign({
      feedback: ({ event }) => event.feedback,
    }),
    // end::assigner[]
  },
}).createMachine({
  id: 'feedback',
  context: { feedback: 'Some feedback', rating: 5 },
  on: { 'feedback.update': { actions: 'updateFeedback' } },
});

const initial = getInitialSnapshot(feedbackMachine);
// tag::event[]
const [next, actions] = transition(feedbackMachine, initial, {
  type: 'feedback.update',
  feedback: 'Some other feedback',
});
// end::event[]
console.log(JSON.stringify(initial.context));
console.log(JSON.stringify(next.context));
console.log(`${actions.length} actions`);
