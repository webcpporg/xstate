// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const feedbackMachine = createMachine({
  id: 'feedback',
  initial: 'question',
  states: {
    question: {
      on: {
        'feedback.good': { target: 'thanks' }
      }
    },
    thanks: {}
  }
});

const event = JSON.parse(
  '{"type": "feedback.good", "feedback": "This is great!", "rating": 5}'
);
const { type, ...payload } = event;
console.log(type, JSON.stringify(payload));

const [question] = initialTransition(feedbackMachine);
const [thanks] = transition(feedbackMachine, question, event);
console.log(JSON.stringify(question.value), '->', JSON.stringify(thanks.value));
