// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const settingsMachine = createMachine({
  id: 'settings',
  type: 'parallel',
  on: {
    'set.dark.custom': { target: ['.mode.dark', '.theme.custom'] },
    'set.light': { target: '.mode.light' }
  },
  states: {
    mode: {
      initial: 'light',
      on: { 'mode.light': '.light' },
      states: {
        light: { on: { toggle: 'dark' } },
        dark: { on: { toggle: 'light' } }
      }
    },
    theme: {
      initial: 'default',
      states: {
        default: { on: { change: 'custom' } },
        custom: { on: { change: 'default' } }
      }
    }
  }
});

let [now] = initialTransition(settingsMachine);
console.log('start', JSON.stringify(now.value));
for (const type of ['set.dark.custom', 'set.light', 'set.dark.custom', 'mode.light']) {
  [now] = transition(settingsMachine, now, { type });
  console.log(type, '->', JSON.stringify(now.value));
}
