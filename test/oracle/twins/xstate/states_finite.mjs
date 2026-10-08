// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const feedbackMachine = createMachine({
  id: 'feedback',
  context: { name: '', email: '', feedback: '' },
  initial: 'prompt',
  states: {
    prompt: { on: { 'feedback.good': 'thanks', 'feedback.bad': 'form' } },
    form: { on: { 'feedback.submit': 'thanks' } },
    thanks: { on: { 'feedback.close': 'closed' } },
    closed: { type: 'final' },
  },
});

const [first] = initialTransition(feedbackMachine);
console.log(JSON.stringify(first.value)); // the finite state
console.log(JSON.stringify(first.context));

const [next] = transition(feedbackMachine, first, { type: 'feedback.good' });
console.log(JSON.stringify(next.value));
