// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { getNextTransitions, initialTransition, setup } from 'xstate';

const targetsOf = (t) =>
  t.target
    ? t.target.map((node) => node.id).join(', ')
    : '(targetless)';

const form = setup({
  guards: {
    hasText: () => true,
    isFull: () => false,
  },
}).createMachine({
  id: 'form',
  initial: 'editing',
  on: { RESET: '.editing' },
  states: {
    editing: {
      on: {
        SUBMIT: { target: 'submitted', guard: 'hasText' },
        TYPE: { actions: 'keep' },
      },
      always: { target: 'submitted', guard: 'isFull' },
    },
    submitted: { type: 'final' },
  },
});

const [editing] = initialTransition(form);
for (const t of getNextTransitions(editing)) {
  console.log(`"${t.eventType}" ${t.source.id} -> ${targetsOf(t)}`);
}
