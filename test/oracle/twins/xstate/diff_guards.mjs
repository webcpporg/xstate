// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { getInitialSnapshot, getNextSnapshot, not, setup } from 'xstate';

const form = setup({
  guards: {
    isEmpty: ({ event }) => !event.name,
  },
}).createMachine({
  id: 'form',
  initial: 'editing',
  states: {
    editing: {
      on: {
        SUBMIT: { target: 'submitted', guard: not('isEmpty') },
      },
    },
    submitted: {},
  },
});

const editing = getInitialSnapshot(form);
const unnamed = getNextSnapshot(form, editing, { type: 'SUBMIT', name: '' });
const named = getNextSnapshot(form, editing, { type: 'SUBMIT', name: 'Ana' });
console.log(`SUBMIT without a name: ${JSON.stringify(unnamed.value)}`);
console.log(`SUBMIT with a name: ${JSON.stringify(named.value)}`);

// The definition as JSON, as a file would hold it.
const definition = JSON.parse(JSON.stringify(form.toJSON()));
const guard = definition.states.editing.on.SUBMIT[0].guard;
console.log(`the definition's guard: ${JSON.stringify(guard)}`);
