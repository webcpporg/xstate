// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

function describe(snapshot) {
  console.log('value:', JSON.stringify(snapshot.value));
  console.log('context:', JSON.stringify(snapshot.context));
  console.log('status:', snapshot.status);
  // _nodes: XState's own list of the active state nodes
  console.log('nodes:', JSON.stringify(snapshot._nodes.map((node) => node.id)));
  console.log('tags:', JSON.stringify([...snapshot.tags]));
  if (snapshot.status === 'done') {
    console.log('output:', JSON.stringify(snapshot.output));
  }
}

const feedbackMachine = createMachine({
  id: 'feedback',
  context: { feedback: '' },
  initial: 'form',
  states: {
    form: {
      tags: ['visible'],
      initial: 'invalid',
      states: {
        invalid: { on: { 'feedback.valid': 'valid' } },
        valid: {},
      },
      on: { 'feedback.submit': 'thanks' },
    },
    thanks: { type: 'final' },
  },
  output: { sent: true },
});

const [first] = initialTransition(feedbackMachine);
describe(first);

const [next] = transition(feedbackMachine, first, { type: 'feedback.submit' });
describe(next);
