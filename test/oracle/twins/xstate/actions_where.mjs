// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getInitialSnapshot, transition } from 'xstate';

const feedbackMachine = createMachine({
  id: 'feedback',
  initial: 'question',
  states: {
    question: {
      exit: 'exitAction',
      on: {
        'feedback.good': { target: 'thanks', actions: 'track' },
      },
    },
    thanks: { entry: 'showConfetti' },
  },
});

const question = getInitialSnapshot(feedbackMachine);
const [thanks, actions] = transition(feedbackMachine, question, {
  type: 'feedback.good',
});
console.log('feedback.good:', actions.map((a) => a.type).join(' '));
