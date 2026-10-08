// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getInitialSnapshot, sendParent, transition } from 'xstate';

const child = createMachine({
  id: 'child',
  on: { FINISH: { actions: sendParent({ type: 'DONE' }) } },
});

try {
  const [next, actions] = transition(child, getInitialSnapshot(child), { type: 'FINISH' });
  for (const returned of actions) {
    console.log(returned.type, JSON.stringify(returned.params));
  }
} catch (error) {
  console.log(`transition() throws: ${error.message}`);
}
