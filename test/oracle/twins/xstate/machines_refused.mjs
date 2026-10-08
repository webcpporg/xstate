// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const form = createMachine({
  initial: 'editing',
  states: {
    editing: {
      on: {
        submit: { guard: 'isValid', target: 'submitted' },
      },
    },
    submitted: {},
  },
});

const [editing] = initialTransition(form);
try {
  transition(form, editing, { type: 'submit' });
} catch (error) {
  console.log(error.message);
}
