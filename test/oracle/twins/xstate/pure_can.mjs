// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, initialTransition, setup, transition } from 'xstate';

const form = setup({
  guards: {
    hasText: ({ context }) => context.text.length > 0,
  },
  actions: {
    keep: assign({ text: ({ event }) => event.text }),
  },
}).createMachine({
  id: 'form',
  initial: 'editing',
  context: { text: '' },
  states: {
    editing: {
      on: {
        SUBMIT: { target: 'submitted', guard: 'hasText' },
        TYPE: { actions: 'keep' },
        IGNORE: {},
      },
    },
    submitted: { type: 'final' },
  },
});

const [editing] = initialTransition(form);
for (const type of ['SUBMIT', 'TYPE', 'IGNORE', 'UNKNOWN']) {
  console.log(`${type}:`, editing.can({ type }));
}

const [typed] = transition(form, editing, {
  type: 'TYPE',
  text: 'hi',
});
console.log('after TYPE, SUBMIT:', typed.can({ type: 'SUBMIT' }));
