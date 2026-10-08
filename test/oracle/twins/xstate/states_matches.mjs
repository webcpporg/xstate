// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, matchesState, transition } from 'xstate';

const feedbackMachine = createMachine({
  id: 'feedback',
  initial: 'question',
  states: {
    question: { on: { 'feedback.bad': 'form' } },
    form: { initial: 'invalid', states: { invalid: {}, valid: {} } },
  },
});

const [snapshot] = transition(
  feedbackMachine,
  initialTransition(feedbackMachine)[0],
  { type: 'feedback.bad' },
);
console.log('value:', JSON.stringify(snapshot.value));

const tested = [
  'question',
  'form',
  { form: 'invalid' },
  { form: 'valid' },
  'form.invalid',
];
for (const stateValue of tested) {
  console.log(`${JSON.stringify(stateValue)}: ${snapshot.matches(stateValue)}`);
}

const playing = { track: 'playing', volume: 'normal' };
const trackPlaying = { track: 'playing' };
console.log(
  `${JSON.stringify(trackPlaying)} in ${JSON.stringify(playing)}: ` +
    `${matchesState(trackPlaying, playing)}`,
);
