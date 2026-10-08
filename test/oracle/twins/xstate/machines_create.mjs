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
        'feedback.good': { target: 'thanks' },
      },
    },
    thanks: {},
  },
});

const [question] = initialTransition(feedbackMachine);
console.log(JSON.stringify(question.value));

const [thanks] = transition(feedbackMachine, question, { type: 'feedback.good' });
console.log(JSON.stringify(thanks.value));
