// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, getInitialSnapshot } from 'xstate';

const feedbackMachine = createMachine({
  id: 'feedback',
  context: {
    feedback: 'Some feedback',
    rating: 5,
  },
});
const snapshot = getInitialSnapshot(feedbackMachine);
console.log(JSON.stringify(snapshot.context));

const toggle = createMachine({ id: 'toggle' });
console.log(JSON.stringify(getInitialSnapshot(toggle).context));
