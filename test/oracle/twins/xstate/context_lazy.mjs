// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getInitialSnapshot } from 'xstate';

let now = 1000;
const feedbackMachine = createMachine({
  id: 'feedback',
  context: () => ({
    feedback: 'Some feedback',
    createdAt: now,
  }),
});

now = 2000;
const first = getInitialSnapshot(feedbackMachine);
now = 3000;
const second = getInitialSnapshot(feedbackMachine);
console.log(JSON.stringify(first.context));
console.log(JSON.stringify(second.context));
