// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, getInitialSnapshot, setup, transition } from 'xstate';

const feedbackMachine = setup({
  actions: {
    addRating: assign(({ context }, params) => ({
      rating: context.rating + params.by,
    })),
  },
}).createMachine({
  id: 'feedback',
  initial: 'question',
  context: { rating: 0 },
  states: {
    question: {
      on: {
        'feedback.good': {
          target: 'thanks',
          actions: [
            { type: 'track', params: { response: 'good' } },
            { type: 'addRating', params: { by: 5 } },
          ],
        },
      },
    },
    thanks: { entry: 'showConfetti' },
  },
});

const question = getInitialSnapshot(feedbackMachine);
const [thanks, actions] = transition(feedbackMachine, question, {
  type: 'feedback.good',
});
for (const action of actions) {
  const params = action.params === undefined ? '' : ` ${JSON.stringify(action.params)}`;
  console.log(`${action.type}${params}`);
}
console.log('context', JSON.stringify(thanks.context));
