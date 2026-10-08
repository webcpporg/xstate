// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const machine = createMachine({
  id: 'machine',
  initial: 'parentState',
  states: {
    parentState: {
      entry: 'enterParent',
      exit: 'exitParent',
      initial: 'someChildState',
      on: {
        'event.targetless': { actions: 'notify' },
        'event.normal': { target: '.someChildState' },
        'event.thatReenters': { target: '.otherChildState', reenter: true },
        'event.self': { target: 'parentState' },
        'event.selfReenters': { target: 'parentState', reenter: true }
      },
      states: {
        someChildState: { entry: 'enterSome', exit: 'exitSome' },
        otherChildState: { entry: 'enterOther', exit: 'exitOther' }
      }
    }
  }
});

const typesOf = (actions) => `[${actions.map((action) => action.type).join(', ')}]`;

let [now, actions] = initialTransition(machine);
console.log('start', JSON.stringify(now.value), typesOf(actions));
for (const type of [
  'event.thatReenters',
  'event.targetless',
  'event.self',
  'event.normal',
  'event.selfReenters'
]) {
  [now, actions] = transition(machine, now, { type });
  console.log(type, '->', JSON.stringify(now.value), typesOf(actions));
}
