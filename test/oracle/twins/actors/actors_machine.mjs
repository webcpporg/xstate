// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createActor, setup } from 'xstate';

const feedbackMachine = setup({}).createMachine({
  id: 'feedback',
  context: ({ input }) => ({ rating: input.defaultRating }),
  initial: 'question',
  states: {
    question: { on: { submit: 'thanks' } },
    thanks: { type: 'final' },
  },
  output: ({ context }) => context,
});

const feedbackActor = createActor(feedbackMachine, {
  input: { defaultRating: 3 },
});
feedbackActor.subscribe((snapshot) => {
  console.log(JSON.stringify(snapshot.context));
});
feedbackActor.start();
// logs {"rating":3}
feedbackActor.send({ type: 'submit' });
// logs {"rating":3}, the snapshot it is done in

const last = feedbackActor.getSnapshot();
console.log('status: ' + last.status);
console.log('output: ' + JSON.stringify(last.output));
