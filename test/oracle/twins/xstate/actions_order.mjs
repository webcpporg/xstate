// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const feedbackMachine = createMachine({
  id: 'feedback',
  initial: 'question',
  entry: 'rootEntry',
  exit: 'rootExit',
  states: {
    question: {
      initial: 'asking',
      entry: 'enterQuestion',
      exit: 'exitQuestion',
      states: {
        asking: {
          entry: 'enterAsking',
          exit: 'exitAsking',
          on: {
            'feedback.good': { target: '#feedback.thanks', actions: 'track' },
          },
        },
      },
    },
    thanks: { type: 'final', entry: 'showConfetti', exit: 'exitThanks' },
  },
});

const [initial, initialActions] = initialTransition(feedbackMachine);
console.log('initial:', initialActions.map((a) => a.type).join(' '));

const [next, actions] = transition(feedbackMachine, initial, {
  type: 'feedback.good',
});
console.log('feedback.good:', actions.map((a) => a.type).join(' '));
