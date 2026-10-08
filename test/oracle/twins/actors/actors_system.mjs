// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { assign, createActor, createMachine, sendTo, setup } from 'xstate';

const notifierMachine = setup({
  actions: { keep: assign({ last: ({ event }) => event.message }) },
}).createMachine({
  context: { last: null },
  on: { notify: { actions: 'keep' } },
});

const formMachine = createMachine({
  on: {
    submit: {
      actions: sendTo(({ system }) => system.get('notifier'), {
        type: 'notify',
        message: 'Form submitted!',
      }),
    },
  },
});

const feedbackMachine = createMachine({
  invoke: [
    { systemId: 'formMachine', src: formMachine },
    { systemId: 'notifier', src: notifierMachine },
  ],
});

const feedbackActor = createActor(feedbackMachine).start();
feedbackActor.system.get('formMachine').send({ type: 'submit' });
const notified = feedbackActor.system.get('notifier').getSnapshot();
console.log(JSON.stringify(notified.context));
// logs {"last":"Form submitted!"}
