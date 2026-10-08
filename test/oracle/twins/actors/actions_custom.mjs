// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, getInitialSnapshot, setup, transition } from 'xstate';

const feedbackMachine = setup({
  actions: {
    track: (_, params) => {
      console.log('tracking', JSON.stringify(params.response));
    },
    showConfetti: () => {
      console.log('confetti!');
    },
  },
}).createMachine({
  id: 'feedback',
  initial: 'question',
  states: {
    question: {
      on: {
        'feedback.good': {
          target: 'thanks',
          actions: [{ type: 'track', params: { response: 'good' } }],
        },
      },
      exit: [{ type: 'exitAction' }],
    },
    thanks: { entry: [{ type: 'showConfetti' }] },
  },
});
const good = { type: 'feedback.good' };

const question = getInitialSnapshot(feedbackMachine);
const [thanks, actions] = transition(feedbackMachine, question, good);
console.log('returned:', actions.map((a) => a.type).join(' '));
for (const action of actions) {
  action.exec?.(action.info, action.params);
}

const actor = createActor(feedbackMachine).start();
console.log('the actor:');
actor.send(good);
