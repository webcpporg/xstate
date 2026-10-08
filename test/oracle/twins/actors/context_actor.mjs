// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, setup } from 'xstate';

const feedbackMachine = setup({
  actions: {
    updateFeedback: assign({
      feedback: ({ event }) => event.feedback,
    }),
  },
}).createMachine({
  id: 'feedback',
  context: { feedback: 'Some feedback' },
  on: { 'feedback.update': { actions: 'updateFeedback' } },
});

const feedbackActor = createActor(feedbackMachine);
feedbackActor.subscribe((state) => {
  console.log(JSON.stringify(state.context));
});
feedbackActor.start();
feedbackActor.send({
  type: 'feedback.update',
  feedback: 'Some other feedback',
});
