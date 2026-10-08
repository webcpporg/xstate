// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { initialTransition, setup, transition } from 'xstate';

const feedbackMachine = setup({
  guards: {
    sentimentGood: ({ event }) => event.rating >= 4,
    sentimentBad: ({ event }) => event.rating <= 2,
    isValid: ({ event }, params) =>
      event.feedback.length > 0 && event.feedback.length <= params.maxLength
  }
}).createMachine({
  id: 'feedback',
  initial: 'prompt',
  states: {
    prompt: {
      on: {
        'feedback.provide': [
          { guard: 'sentimentGood', target: 'thanks' },
          { guard: 'sentimentBad', target: 'form' },
          { target: 'neutral' }
        ]
      }
    },
    form: {
      on: {
        submit: {
          guard: { type: 'isValid', params: { maxLength: 50 } },
          target: 'submitting'
        }
      }
    },
    thanks: {},
    neutral: {},
    submitting: {}
  }
});

const [prompt] = initialTransition(feedbackMachine);
for (const rating of [5, 1, 3]) {
  const [next] = transition(feedbackMachine, prompt, { type: 'feedback.provide', rating });
  console.log('rating', rating, '->', JSON.stringify(next.value));
}

const [form] = transition(feedbackMachine, prompt, { type: 'feedback.provide', rating: 1 });
for (const feedback of ['', 'Too slow.']) {
  const [next] = transition(feedbackMachine, form, { type: 'submit', feedback });
  console.log('feedback', JSON.stringify(feedback), '->', JSON.stringify(next.value));
}

try {
  transition(feedbackMachine, form, { type: 'submit' });
} catch (error) {
  console.log('no feedback ->', error.message);
}
