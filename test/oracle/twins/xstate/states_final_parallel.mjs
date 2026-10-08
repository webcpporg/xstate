// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

import { createMachine, initialTransition, transition } from 'xstate';

const coffeeMachine = createMachine({
  id: 'coffee',
  initial: 'preparing',
  states: {
    preparing: {
      type: 'parallel',
      states: {
        grindBeans: {
          initial: 'grindingBeans',
          states: {
            grindingBeans: { on: { BEANS_GROUND: 'beansGround' } },
            beansGround: { type: 'final' },
          },
        },
        boilWater: {
          initial: 'boilingWater',
          states: {
            boilingWater: { on: { WATER_BOILED: 'waterBoiled' } },
            waterBoiled: { type: 'final' },
          },
        },
      },
      onDone: 'makingCoffee',
    },
    makingCoffee: {},
  },
});

let [snapshot] = initialTransition(coffeeMachine);
console.log(JSON.stringify(snapshot.value));

[snapshot] = transition(coffeeMachine, snapshot, { type: 'BEANS_GROUND' });
console.log(JSON.stringify(snapshot.value));

[snapshot] = transition(coffeeMachine, snapshot, { type: 'WATER_BOILED' });
console.log(JSON.stringify(snapshot.value));
