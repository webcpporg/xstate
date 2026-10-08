// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const feedbackMachine = createMachine({
  id: 'feedback',
  initial: 'prompt',
  states: {
    prompt: {
      on: {
        'feedback.good': 'thanks',
        'feedback.*': 'form',
        'feedback.bad.*': 'apology',
        '*': 'other'
      }
    },
    thanks: {},
    form: {},
    apology: {},
    other: {}
  }
});

const [prompt] = initialTransition(feedbackMachine);
for (const type of [
  'feedback.good',
  'feedback.bad',
  'feedback.bad.rude',
  'feedback.meh',
  'feedback',
  'feedbacks',
  'mouse.click',
  '*'
]) {
  try {
    const [next] = transition(feedbackMachine, prompt, { type });
    console.log(type, '->', JSON.stringify(next.value));
  } catch (error) {
    console.log(type, '->', error.message);
  }
}
