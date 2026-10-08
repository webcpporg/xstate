// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

function show(snapshot) {
  console.log(
    JSON.stringify(snapshot.value),
    JSON.stringify([...snapshot.tags]),
    `visible: ${snapshot.hasTag('visible')}`,
  );
}

const feedbackMachine = createMachine({
  id: 'feedback',
  initial: 'prompt',
  states: {
    prompt: { tags: ['visible'], on: { 'feedback.good': 'thanks' } },
    thanks: {
      tags: ['visible'],
      initial: 'celebrating',
      states: { celebrating: { tags: ['visible', 'confetti'] } },
      on: { 'feedback.close': 'closed' },
    },
    closed: { tags: 'hidden' },
  },
});

let [snapshot] = initialTransition(feedbackMachine);
show(snapshot);

[snapshot] = transition(feedbackMachine, snapshot, { type: 'feedback.good' });
show(snapshot);

[snapshot] = transition(feedbackMachine, snapshot, { type: 'feedback.close' });
show(snapshot);
