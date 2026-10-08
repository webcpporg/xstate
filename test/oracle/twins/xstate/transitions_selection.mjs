// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const editorMachine = createMachine({
  id: 'editor',
  initial: 'editing',
  states: {
    editing: {
      initial: 'text',
      on: { escape: 'closed' },
      states: {
        text: { on: { 'menu.open': 'menu', save: 'saving' } },
        menu: { on: { escape: 'text' } },
        saving: { on: { escape: {} } }
      }
    },
    closed: {}
  }
});

for (const run of [['menu.open', 'escape', 'escape'], ['save', 'escape']]) {
  let [now] = initialTransition(editorMachine);
  console.log('start', JSON.stringify(now.value));
  for (const type of run) {
    [now] = transition(editorMachine, now, { type });
    console.log(type, '->', JSON.stringify(now.value));
  }
}
