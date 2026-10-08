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
    question: { on: { next: 'form' } },
    form: {
      initial: 'invalid',
      states: { invalid: {}, valid: {} },
      on: { settings: 'settings' },
    },
    settings: {
      type: 'parallel',
      states: {
        display: { initial: 'bright', states: { bright: {}, dim: {} } },
        theme: { initial: 'dark', states: { dark: {}, light: {} } },
      },
    },
  },
});

let [snapshot] = initialTransition(feedbackMachine);
console.log(JSON.stringify(snapshot.value));

[snapshot] = transition(feedbackMachine, snapshot, { type: 'next' });
console.log(JSON.stringify(snapshot.value));

[snapshot] = transition(feedbackMachine, snapshot, { type: 'settings' });
console.log(JSON.stringify(snapshot.value));

const countingMachine = createMachine({ id: 'counting' });
console.log(JSON.stringify(initialTransition(countingMachine)[0].value));
